#include <stdio.h>

#include <signal.h>
#include <setjmp.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#define PRESSURE_SCALE 0.0091558323
#define LENGTH_SCALE 3.906379093e-4

static void *pruDataMem0;
static void *pruDataMem1;
static unsigned int *pruDataMem0_int;
static unsigned int *pruDataMem1_int;

static jmp_buf buf;

void signalCatcher (int null)
{
	longjmp (buf, 1);
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
	int spiFile1;
	int spiFile2;
	int pressHandle1;
	int pressHandle2;
	int temp;
	float force;
	float length1;
	float length2;
	float pressure1;
	float pressure2;
	struct timespec waitTime = {0, 100000000};

	if (setjmp (buf))
		goto shutdown;

	signal (SIGINT, signalCatcher);

	pruInit ();

	if (prussdrv_exec_program (1, "./coldStart.bin")) {
		fprintf (stderr, "prussdrv_exec_program(1, './coldStart.bin') failed\n");
		return 1;
	}
	puts ("Making sure all valves are closed before starting");
	sleep (10);
	puts ("Starting");

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

	while (1) {
		temp = pruDataMem0_int[0];
		if (temp & (1 << 15))
			temp |= 0xFFFF0000;
		length1 = (float)temp * LENGTH_SCALE;

		temp = pruDataMem0_int[1];
		if (temp & (1 << 15))
			temp |= 0xFFFF0000;
		length2 = (float)temp * LENGTH_SCALE;

		pruDataMem1_int[0] = (int)(length1 * 100.0);
		pruDataMem1_int[1] = (int)(length2 * 100.0);

		printf ("%d, %d\n", (int)(length1 * 100.0), (int)(length2 * 100.0));
	}

/*
	while (1) {
		temp = pruDataMem0_int[0];
		if (temp & (1 << 15))
			temp |= 0xFFFF0000;
		length1 = (float)temp * LENGTH_SCALE;
		temp = pruDataMem0_int[1];
		if (temp & (1 << 15))
			temp |= 0xFFFF0000;
		length2 = (float)temp * LENGTH_SCALE;
		printf ("%f, %f\n", length1, length2);
	}
*/

/*
	while (1) {
		nanosleep (&waitTime, NULL);
		temp = i2cRead (pressHandle1);
		pressure1 = (float)temp * PRESSURE_SCALE;
		temp = i2cRead (pressHandle2);
		pressure2 = (float)temp * PRESSURE_SCALE;
		printf ("%f, %f\n", pressure1, pressure2);
	}
*/

shutdown:

	pruTerminate ();

	return 0;
}
