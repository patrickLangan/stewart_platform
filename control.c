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

void printBinary (int num)
{
	int i;

	for (i = 0; i < 33; i++) {
		printf ("%d", num & 1);
		num >>= 1;
	}
	puts ("");
}

int convert16to32bit (int num)
{
	if (0b1000000000000000 & num)
		num |= 0b11111111111111110000000000000000;

	return num;
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

	if (prussdrv_exec_program (1, "./stepper.bin")) {
		fprintf (stderr, "prussdrv_exec_program (1, './stepper.bin') failed\n");
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

int controlValveInit (struct valveInfo *controlValve)
{
	int i;
	char gpioPath[30];

	for (i = 0; i < 6; i++) {
		sprintf (gpioPath, "/sys/class/gpio/gpio%d/value", controlValve[i].gpio);
		controlValve[i].file = fopen (gpioPath, "w");
		fprintf (controlValve[i].file, "0");
		fflush (controlValve[i].file);
	}

	return 0;
}

int controlValveClose (struct valveInfo *controlValve)
{
	int i;

	for (i = 0; i < 6; i++) {
		fprintf (controlValve[i].file, "0");
		fclose (controlValve[i].file);
	}

	return 0;
}

float joystickRead (FILE *file)
{
	int value;

	fscanf (file, "%d", &value);
	fflush (file);
	fseek (file, 0, SEEK_SET);

	value -= 960;

	if (abs (value) < 10)
		return 0.0;
	else
		return (float)value / 960.0;

}

int accelInit (void)
{
	int handle;
	char buffer[2];

	handle = i2c_open (1, 0x53);

	i2c_write_byte (handle, 0x00);
	i2c_read (handle, buffer, 1);
	if ((int)*buffer != 0xE5) {
		fprintf (stderr, "Could not connect to accelerometer\n");
		return -1;
	}

	buffer[0] = 0x31;
	buffer[1] = 0x00;
	i2c_write (handle, buffer, 2);

	buffer[0] = 0x2D;
	buffer[1] = 0x08;
	i2c_write (handle, buffer, 2);

	return handle;
}

struct vector accelRead (int handle)
{
	char buffer[6];
	int rawX, rawY, rawZ;
	struct vector accel;

	i2c_write_byte (handle, 0x32);
	i2c_read (handle, buffer, 6);
	rawX = ((int)buffer[1] << 8) | (int)buffer[0];
	rawY = ((int)buffer[3] << 8) | (int)buffer[2];
	rawZ = ((int)buffer[5] << 8) | (int)buffer[4];

	accel.x = (float)convert16to32bit (rawX) * ACCEL_SCALE;
	accel.y = (float)convert16to32bit (rawY) * ACCEL_SCALE;
	accel.z = (float)convert16to32bit (rawZ) * ACCEL_SCALE;

	return (struct vector) {accel.x, accel.y, accel.z};
}

float singleAccelRead (int handle, int address)
{
	char buffer[2];
	int raw;

	i2c_write_byte (handle, address);
	i2c_read (handle, buffer, 2);
	raw = ((int)buffer[1] << 8) | (int)buffer[0];

	return (float)convert16to32bit (raw) * ACCEL_SCALE;
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
	int handleAccel;
	int handlePress1;
	int handlePress2;

	float acceleration;
	float pressure1;
	float pressure2;

	FILE *file;

	struct timeval initTime;
	float time;

	struct valveInfo controlValve[6] = {{89}, {10}, {11}, {9}, {81}, {8}};

	FILE *joystickFile;
	float joystick;

	int i;

	if (setjmp (buf))
		goto shutdown;

	signal (SIGINT, signalCatcher);

	controlValveInit (controlValve);
	pruOpen ();
	joystickFile = fopen ("/sys/devices/ocp.3/helper.13/AIN1", "r");

	while (1) {
		joystick = joystickRead (joystickFile);
		printf ("%f\n", joystick);
/*
		for (i = 0; i < 6; i++) {
			fprintf (controlValve[i].file, "1");
			fflush (controlValve[i].file);
		}
		sleep (2);
		for (i = 0; i < 6; i++) {
			fprintf (controlValve[i].file, "0");
			fflush (controlValve[i].file);
		}
		sleep (2);
*/
	}

/*
	if (
		((argc < 2) ? fprintf (stderr, "You need to pass a file name for data to be written to.\n") : 0) ||
		((argc > 2) ? fprintf (stderr, "Too many parameters.\n") : 0)
	)
		return 1;

	file = fopen (argv[1], "w");

	handleAccel = accelInit ();
	handlePress1 = i2c_open (1, 0x28);
	handlePress2 = i2c_open (2, 0x28);

	fprintf (file, "time (ms), pressure1, pressure2, accel (g)\n");

	gettimeofday (&initTime, NULL);

	while (1) {
		pressure1 = pressureRead (handlePress1);
		pressure2 = pressureRead (handlePress2);
		acceleration = singleAccelRead (handleAccel, 0x34);

		time = gettimefromfunction (initTime);

		fprintf (file, "%f, %f, %f, %f\n", time, pressure1, pressure2, acceleration);
	}
*/

shutdown:

	//fclose (file);
	fclose (joystickFile);
	controlValveClose (controlValve);
	pruClose ();

	return 0;
}


