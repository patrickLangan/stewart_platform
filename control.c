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

int joystickRead (FILE *file)
{
	int value;

	fscanf (file, "%d", &value);
	fflush (file);
	fseek (file, 0, SEEK_SET);

	value -= 960;

	return value;
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
	struct valveInfo controlValve[6] = {{89}, {10}, {11}, {9}, {81}, {8}};
	FILE *joystickFile;
	int i;

	if (setjmp (buf))
		goto shutdown;

	signal (SIGINT, signalCatcher);

	controlValveInit (controlValve);
	joystickFile = fopen ("/sys/devices/ocp.3/helper.13/AIN1", "r");

	pruOpen ();

	prussdrv_map_prumem (PRUSS0_PRU1_DATARAM, &pruDataMem);
	pruDataMem_int = (unsigned int*) pruDataMem;

	if (prussdrv_exec_program (1, "./stepper.bin")) {
		fprintf (stderr, "prussdrv_exec_program (1, './stepper.bin') failed\n");
		return 1;
	}

	while (1) {
		int joystick = 0;
		static int dir = 0;

		for (i = 0; i < 100; i++)
			joystick += joystickRead (joystickFile);

		if (abs (joystick) < 1000)
			joystick = 0;
		else if (joystick < 0)
			joystick *= JOYSTICK_SCALE_UP;
		else
			joystick *= JOYSTICK_SCALE_DOWN;

		pruDataMem_int[0] = abs (joystick);
		pruDataMem_int[1] = abs (joystick);

		if (joystick < 0) {
			if (dir == 0) {
				usleep (10000);
				fprintf (controlValve[0].file, "1");
				fflush (controlValve[0].file);
				dir = 1;
			}
		} else {
			if (dir == 1) {
				usleep (200000);
				fprintf (controlValve[0].file, "0");
				fflush (controlValve[0].file);
				dir = 0;
			}
		}
	}

shutdown:

	fclose (joystickFile);
	controlValveClose (controlValve);
	pruClose ();

	return 0;
}


