#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <setjmp.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#define OFFSET_SHAREDRAM 2048

struct axisInfo {
	FILE *run;
	FILE *duty;
	FILE *period;
	FILE *direction;
};

struct vector {
	float x;
	float y;
	float z;
};

static jmp_buf buf;

static unsigned int *sharedMem_int;

struct axisInfo xAxis;
struct axisInfo yAxis;
struct axisInfo zAxis;

struct vector sensorInit = {0.0, 0.0, 0.0};

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

void PRUWait (void)
{
	prussdrv_pru_wait_event (PRU_EVTOUT_0);
	prussdrv_pru_clear_event (PRU0_ARM_INTERRUPT, PRU0_ARM_INTERRUPT);
}

void PRUInit (void)
{
	tpruss_intc_initdata pruss_intc_initdata=PRUSS_INTC_INITDATA;
	static void *sharedMem;

	prussdrv_init();

	if (prussdrv_open (PRU_EVTOUT_0)) {
		puts ("Failed to initialize the PRU's memory mapping");
		exit (1);
	}

	if (prussdrv_pruintc_init (&pruss_intc_initdata) == -1) {
		puts ("Failed to initialize the PRU's interrupt controller");
		exit (1);
	}

	if (prussdrv_exec_program (0, "./sensor.bin") == -1) {
		puts ("Failed to execute PRU program");
		exit (1);
	}

	prussdrv_map_prumem (PRUSS0_SHARED_DATARAM, &sharedMem);
	sharedMem_int = (unsigned int *)sharedMem;
}

float gettimefromfunction (struct timeval startTime)
{
	struct timeval curTime;

	gettimeofday (&curTime, NULL);

	return ((float)(curTime.tv_sec - startTime.tv_sec) * 1e3) + ((float)(curTime.tv_usec - startTime.tv_usec) * 1e-3);
}

FILE *openLine (FILE *pipe, unsigned int length)
{
	char line[99];

	fgets (line, 99, pipe);
	line[length] = '\0';

	return fopen (line, "w");
}

void startupAxis (struct axisInfo *axis, unsigned int number, char letter)
{
	FILE *pipe;
	char bashCommand[22];

	sprintf (bashCommand, "./startupAxis.sh %d %c", number, letter);
	pipe = popen (bashCommand, "r");

	axis->run = openLine (pipe, 36);
	axis->duty = openLine (pipe, 37);
	axis->period = openLine (pipe, 39);
	axis->direction = openLine (pipe, 28);

	pclose (pipe);
}

void shutdownAxis (struct axisInfo *axis)
{
	fprintf (axis->run, "0");
	fclose (axis->run);

	fprintf (axis->duty, "0");
	fclose (axis->duty);

	fclose (axis->period);

	fclose (axis->direction);
}

void readSensors (float *x, float *y, float *z)
{
	int resultX = sharedMem_int[OFFSET_SHAREDRAM + 0];
	int resultY = sharedMem_int[OFFSET_SHAREDRAM + 1];
	int resultZ = sharedMem_int[OFFSET_SHAREDRAM + 2];

	if (resultX >= 32768) {
		resultX |= 4294901760LL;
	}
	if (resultY >= 32768) {
		resultY |= 4294901760LL;
	}
	if (resultZ >= 32768) {
		resultZ |= 4294901760LL;
	}

	*x = 0.0003904191149 * ((float)resultX);
	*y = 0.0003904191149 * ((float)resultY);
	*z = 0.0003904191149 * ((float)resultZ);
}

void initSensors (void)
{
	unsigned int initPointNum = 100;
	float *initArrayX;
	float *initArrayY;
	float *initArrayZ;
	unsigned int i;

	initArrayX = malloc (initPointNum * sizeof(*initArrayX));
	initArrayY = malloc (initPointNum * sizeof(*initArrayY));
	initArrayZ = malloc (initPointNum * sizeof(*initArrayZ));

	for (i = initPointNum; i; i--) {
		readSensors (&initArrayX[i], &initArrayY[i], &initArrayZ[i]);
		PRUWait ();
	}

	qsort (initArrayX, initPointNum, sizeof(*initArrayX), compareFloats);
	sensorInit.x = initArrayX[50];

	qsort (initArrayY, initPointNum, sizeof(*initArrayY), compareFloats);
	sensorInit.y = initArrayY[50];

	qsort (initArrayZ, initPointNum, sizeof(*initArrayZ), compareFloats);
	sensorInit.z = initArrayZ[50];

	free (initArrayX);
	free (initArrayY);
	free (initArrayZ);
}

int i2cInit (void)
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

void i2cRead (int handle)
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

	accel.x = rawX * 0.00390625;
	accel.y = rawY * 0.00390625;
	accel.z = rawZ * 0.00390625;

	printf ("%f, %f, %f\n", accel.x, accel.y, accel.z);
}

void setVelocity (struct axisInfo *axis, float *lastDir, float velocity)
{
	unsigned int period;
	unsigned int duty;

	if (velocity < 0.0) {
		velocity = fabs (velocity);
		if (*lastDir != 0.0) {
			*lastDir = 0.0;
			fprintf (axis->direction, "0");
			fflush (axis->direction);
		}
	} else {
		if (*lastDir != 1.0) {
			*lastDir = 1.0;
			fprintf (axis->direction, "1");
			fflush (axis->direction);
		}
	}

	period = (int)(500000.0 / velocity);
	duty = (int)(250000.0 / velocity);

	fprintf (axis->duty, "0");
	fflush (axis->duty);
	fprintf (axis->period, "%d", period);
	fflush (axis->period);
	fprintf (axis->duty, "%d", duty);
	fflush (axis->duty);
}

void turnValve (float turns, struct axisInfo *axis)
{
	static struct vector lastDir = {1.0, 1.0, 1.0};
	struct timespec req, rem;

	if (turns == 0)
		goto out;

	if (turns < 0) {
		setVelocity (axis, &lastDir.x, 0.1);
		turns = -turns;
	} else {
		setVelocity (axis, &lastDir.x, -0.1);
	}

	req.tv_sec = ((int)turns) / 2; 
	req.tv_nsec = (int)(fmod (turns, 2) * 5e8); 

	fprintf (axis->run, "1");
	fflush (axis->run);

	nanosleep (&req, &rem);

	fprintf (axis->run, "0");
	fflush (axis->run);

	setVelocity (axis, &lastDir.x, 0.0);

out:;
}

int main (int argc, char **argv)
{
	int i;

	struct vector sensor;

	int handle;

	char word[255];
	float position = 0;
	float setPosition;

	if (setjmp (buf))
		goto shutdown;

	signal (SIGINT, signalCatcher);

	startupAxis (&xAxis, 30, 'x');
	startupAxis (&yAxis, 60, 'y');
	startupAxis (&zAxis, 31, 'z');

	fprintf (xAxis.run, "0");
	fflush (xAxis.run);

/*
	PRUInit ();
	PRUWait ();

	initSensors ();
*/

	handle = i2cInit ();

	while (1) {
		/*
		readSensors (&sensor.x, &sensor.y, &sensor.z);
		sensor = subVec (sensor, sensorInit);
		printf ("%f, %f, %f\n", sensor.x, sensor.y, sensor.z);
		*/

		printf (":");
		scanf ("%s", word);
		if (strcmp ("set", word) == 0) {
			scanf ("%f", &setPosition);

			if (setPosition < 0 || setPosition > 13) {
				puts ("Out of range");
			} else {
				turnValve (setPosition - position, &xAxis);
				position = setPosition;
			}
    		} else if (strcmp ("open", word) == 0) {
			turnValve (13 - position, &xAxis);
			position = 13;
    		} else if (strcmp ("close", word) == 0) {
			turnValve (-position, &xAxis);
			position = 0;
		} else if (strcmp ("exit", word) == 0) {
			goto shutdown;
		} else {
			puts ("Bad input");
		}
	}

shutdown:
	shutdownAxis (&xAxis);
	shutdownAxis (&yAxis);
	shutdownAxis (&zAxis);

/*
	prussdrv_pru_disable (0);
	prussdrv_exit ();
*/

	return 0;
}

