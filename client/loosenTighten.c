#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <signal.h>
#include <setjmp.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#define PRESSURE_SCALE 0.0091558323
#define LENGTH_SCALE 3.123433875e-4
#define LENGTH_ZERO 993

#define BUFF_LEN 10

static void *pruDataMem0;
static void *pruDataMem1;
static unsigned int *pruDataMem0_int;
static unsigned int *pruDataMem1_int;

static jmp_buf buf;

void signalCatcher (int null)
{
        longjmp (buf, 1);
}

int compareFloats (const void *a, const void *b)
{
	const float *fa = (const float *)a;
	const float *fb = (const float *)b;

	return (*fa > *fb) - (*fa < *fb);
}

int pruInit (void)
{
        tpruss_intc_initdata intc = PRUSS_INTC_INITDATA;

        if (prussdrv_init ()) {
                fprintf (stderr, "prussdrw_init() failed\n");
                return 1;
        }

        if (prussdrv_open (PRU_EVTOUT_0)) {
                fprintf (stderr, "prussdrv_open(PRU_EVTOUT_0) failed\n");
                return 1;
        }

        if (prussdrv_open (PRU_EVTOUT_1)) {
                fprintf (stderr, "prussdrv_open(PRU_EVTOUT_1) failed\n");
                return 1;
        }

        return 0;
}


int pruTerminate (void)
{
        if (prussdrv_pru_disable (0)) {
                fprintf (stderr, "prussdrv_pru_disable(0) failed\n");
                return 1;
        }

        if (prussdrv_pru_disable (1)) {
                fprintf (stderr, "prussdrv_pru_disable(1) failed\n");
                return 1;
        }

        if (prussdrv_exit()) {
                fprintf (stderr, "prussdrv_exit() failed\n");
                return 1;
        }

        return 0;
}

int i2cRead (int handle)
{
        char buffer[2];

        i2c_read (handle, buffer, 2);

        return (int)buffer[0] << 8 | (int)buffer[1];
}

int main (int argc, char **argv)
{
        float pressure1;
        float pressure2;
	float length;

	int lengthHandle;
        int pressHandle1;
        int pressHandle2;

	float buffer[BUFF_LEN] = {0.0};

        int temp;
	int i;

        if (setjmp (buf))
                goto shutdown;

        signal (SIGINT, signalCatcher);

        pruInit ();

        prussdrv_map_prumem (PRUSS0_PRU0_DATARAM, &pruDataMem0);
        pruDataMem0_int = (unsigned int*) pruDataMem0;

        prussdrv_map_prumem (PRUSS0_PRU1_DATARAM, &pruDataMem1);
        pruDataMem1_int = (unsigned int*) pruDataMem1;

        if (prussdrv_exec_program (0, "./length.bin")) {
                fprintf (stderr, "prussdrv_exec_program(0, './length.bin') failed\n");
                return 1;
        }

        if (prussdrv_exec_program (1, "./stepper.bin")) {
                fprintf (stderr, "prussdrv_exec_program(1, './stepper.bin') failed\n");
                return 1;
        }

        pressHandle1 = i2c_open (1, 0x28);
        pressHandle2 = i2c_open (2, 0x28);

	pruDataMem1_int[0] = -100;
	pruDataMem1_int[1] = 0;

	while (1) {
		for (i = 0; i < BUFF_LEN - 1; i++)
			buffer[i] = buffer[i + 1];

		temp = i2cRead (pressHandle1);
		buffer[BUFF_LEN - 1] = (float)temp * PRESSURE_SCALE;

		qsort (buffer, BUFF_LEN, sizeof(*buffer), compareFloats);
		pressure1 = buffer[BUFF_LEN / 2];

		if (pressure1 > 80.0)
			break;

		usleep (5000);
	}

	pruDataMem1_int[0] = 0;
	pruDataMem1_int[1] = 100;

	memset (buffer, 0, sizeof(buffer));
	while (1) {
		for (i = 0; i < BUFF_LEN - 1; i++)
			buffer[i] = buffer[i + 1];

		temp = i2cRead (pressHandle2);
		buffer[BUFF_LEN - 1] = (float)temp * PRESSURE_SCALE;

		qsort (buffer, BUFF_LEN, sizeof(*buffer), compareFloats);
		pressure2 = buffer[BUFF_LEN / 2];

		temp = pruDataMem0_int[0];
		length = (float)(temp - LENGTH_ZERO) * LENGTH_SCALE;

		if (pressure2 > 80.0 || length > 0.0762)
			break;

		usleep (5000);
	}

shutdown:
	pruDataMem1_int[0] = 0;
	pruDataMem1_int[1] = 0;
	sleep (3);

        pruTerminate ();

	i2c_close (pressHandle1);
	i2c_close (pressHandle2);

        return 0;
}

