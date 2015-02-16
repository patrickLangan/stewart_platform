#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <setjmp.h>
#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#define ACCEL_SCALE	0.0039100684
#define PRESSURE_SCALE	0.0091558323

#define JOYSTICK_SCALE_UP	0.01
#define JOYSTICK_SCALE_DOWN	0.0075

static void *pruDataMem;
static unsigned int *pruDataMem_int;

struct vector {
	float x;
	float y;
	float z;
};

struct valveInfo {
	int gpio;
	int value;
	FILE *file;
};

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

float gettimefromfunction (struct timeval startTime)
{
	struct timeval curTime;

	gettimeofday (&curTime, NULL);

	return ((float)(curTime.tv_sec - startTime.tv_sec) * 1e3) + ((float)(curTime.tv_usec - startTime.tv_usec) * 1e-3);
}

int pruOpen (void)
{
	tpruss_intc_initdata intc = PRUSS_INTC_INITDATA;

	if (prussdrv_init ()) {
		fprintf (stderr, "prussdrw_init () failed\n");
		return 1;
	}

	if (prussdrv_open (PRU_EVTOUT_1)) {
		fprintf (stderr, "prussdrv_open (PRU_EVTOUT_1) failed\n");
		return 1;
	}

	return 0;
}


int pruClose (void)
{
	if (prussdrv_pru_disable (1)) {
		fprintf (stderr, "prussdrv_pru_disable (1) failed\n");
		return 1;
	}

	if (prussdrv_exit()) {
		fprintf (stderr, "prussdrv_exit () failed\n");
		return 1;
	}

	return 0;
}

int joystickRead (FILE *file)
{
	int value;

	fscanf (file, "%d", &value);
	fflush (file);
	fseek (file, 0, SEEK_SET);

	value -= 960;

	return value;
}

float pressureRead (int handle)
{
	char buffer[2];
	int raw;

	i2c_read (handle, buffer, 2);
	raw = ((int)buffer[0] << 8) | (int)buffer[1];

	return (float)raw * PRESSURE_SCALE;
}

int main (int argc, char **argv)
{
	FILE *joystickFile;
	int i;

	if (setjmp (buf))
		goto shutdown;

	signal (SIGINT, signalCatcher);

	joystickFile = fopen ("/sys/devices/ocp.3/helper.13/AIN1", "r");

	pruOpen ();

	prussdrv_map_prumem (PRUSS0_PRU1_DATARAM, &pruDataMem);
	pruDataMem_int = (unsigned int*) pruDataMem;

	if (prussdrv_exec_program (1, "./stepper.bin")) {
		fprintf (stderr, "prussdrv_exec_program (1, './stepper.bin') failed\n");
		return 1;
	}

	pruDataMem_int[6] = 0;
	while (1) {
		int joystick = 0;

		//Average of 100 readings from the joystick
		for (i = 0; i < 100; i++)
			joystick += joystickRead (joystickFile);

		//Throws out small values (to avoid jittery motion around zero) and scales up or down joystick readings
		if (abs (joystick) < 1000)
			joystick = 0;
		else if (joystick < 0)
			joystick *= JOYSTICK_SCALE_UP;
		else
			joystick *= JOYSTICK_SCALE_DOWN;

		//Sets positions for the 12 motors (replace joystick with motorPos[i])
		for (i = 0; i < 12; i++)
			pruDataMem_int[i] = abs (joystick);

		//Sets directions for the six control valves (replace joystick with motorPos[i])
		for (i = 0; i < 6; i++) {
			if (joystick < 0)
				pruDataMem_int[6] |= 1 << i;
			else if (joystick > 0)
				pruDataMem_int[6] &= ~(1 << i);
		}
	}

shutdown:

	fclose (joystickFile);
	pruClose ();

	return 0;
}

