#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <setjmp.h>

struct vector {
	float x;
	float y;
	float z;
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

int accelInit (void)
{
	int handle;
	char buffer[2];

	handle = i2c_open (1, 0x53);

	i2c_write_byte (handle, 0x00);
	i2c_read (handle, buffer, 1);
	if ((int)*buffer != 0xE5) {
		fprintf (stderr, "Could not connect to accelerometer");
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

void accelRead (int handle)
{
	int rawX, rawY, rawZ;
	struct vector accel;
	char buffer[6];

	i2c_write_byte (handle, 0x32);
	i2c_read (handle, buffer, 6);
	rawX = ((int)buffer[1] << 8) | (int)buffer[0];
	rawY = ((int)buffer[3] << 8) | (int)buffer[2];
	rawZ = ((int)buffer[5] << 8) | (int)buffer[4];

	if (0b1000000000000000 & rawX)
		rawX |= 0b11111111111111110000000000000000;
	if (0b1000000000000000 & rawY)
		rawY |= 0b11111111111111110000000000000000;
	if (0b1000000000000000 & rawZ)
		rawZ |= 0b11111111111111110000000000000000;

	accel.x = (float)rawX * 0.0039100684;
	accel.y = (float)rawY * 0.0039100684;
	accel.z = (float)rawZ * 0.0039100684;

	printf ("%f, %f, %f\n", accel.x, accel.y, accel.z);
}

void pressureRead (int handle)
{
	int raw;
	float pressure;
	char buffer[2];

	i2c_read (handle, buffer, 2);
	raw = ((int)buffer[0] << 8) | (int)buffer[1];
	pressure = (float)raw * 0.0091558323;

	printf ("%f, ", pressure);
}

int main (int argc, char **argv)
{
	int accel;
	int pressure1;
	int pressure2;

	if (setjmp (buf))
		goto shutdown;

	signal (SIGINT, signalCatcher);

	accel = accelInit ();
	pressure1 = i2c_open (1, 0x28);
	pressure2 = i2c_open (2, 0x28);

	while (1) {
		pressureRead (pressure1);
		pressureRead (pressure2);
		accelRead (accel);
	}

shutdown:

	return 0;
}


