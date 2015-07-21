#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <time.h>

#include <signal.h>
#include <setjmp.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>

static float maxVelocity = 39.37e-6; //(in/(micro s))

static int spiBits = 8;
static int spiSpeed = 500000;

static float potScale = 0.0120147945;

static int motor[12] = {6, 7, 8, 9, 10, 11, 5, 4, 3, 2, 1, 0};

static void *pruDataMem;
static unsigned int *pruDataMem_int;

static jmp_buf buf;

struct gpioInfo {
	int pin;
	int value;
	FILE *file;
};

void signalCatcher (int null)
{
	longjmp (buf, 1);
}

int gettimefromfunction (struct timeval startTime)
{
	struct timeval curTime;

	gettimeofday (&curTime, NULL);

	return ((curTime.tv_sec - startTime.tv_sec) * 1e6) + curTime.tv_usec - startTime.tv_usec;
}

int pruInit (void)
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


int pruAbscond (void)
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

int gpioOutputInit (struct gpioInfo *gpio, char *value)
{
        char gpioPath[30];

        sprintf (gpioPath, "/sys/class/gpio/gpio%d/value", gpio->pin);
        gpio->file = fopen (gpioPath, "w");
        fprintf (gpio->file, value);
        fflush (gpio->file);

        return 0;
}

int gpioOutputAbscond (struct gpioInfo *gpio, char *value)
{
        fprintf (gpio->file, value);
        fclose (gpio->file);
        return 0;
}

int gpioInputInit (struct gpioInfo *gpio)
{
        char gpioPath[30];

        sprintf (gpioPath, "/sys/class/gpio/gpio%d/value", gpio->pin);
        gpio->file = fopen (gpioPath, "r");

        return 0;
}

int gpioInputAbscond (struct gpioInfo *gpio)
{
        fclose (gpio->file);
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

void spiInit (char *file, int *fd)
{
	*fd = open (file, O_RDWR);
	ioctl (*fd, SPI_IOC_WR_BITS_PER_WORD, &spiBits);
	ioctl (*fd, SPI_IOC_RD_BITS_PER_WORD, &spiBits);
	ioctl (*fd, SPI_IOC_WR_MAX_SPEED_HZ, &spiSpeed);
	ioctl (*fd, SPI_IOC_RD_MAX_SPEED_HZ, &spiSpeed);
}

void spiRead (int fd1, int fd2, FILE *cs, float *out)
{
	uint8_t tx[2] = {1, 1};
	uint8_t rx[2];
	struct spi_ioc_transfer tr;

	tr.tx_buf = (unsigned long)tx;
	tr.rx_buf = (unsigned long)rx;
	tr.len = 2;
	tr.speed_hz = spiSpeed;
	tr.bits_per_word = spiBits;

	fprintf (cs, "0");
	fflush (cs);

	ioctl (fd1, SPI_IOC_MESSAGE (1), &tr);
        out[0] = (float)((((rx[0] << 8) | rx[1]) >> 1) & 0b0000111111111111) * potScale;

	ioctl (fd2, SPI_IOC_MESSAGE (1), &tr);
        out[1] = (float)((((rx[0] << 8) | rx[1]) >> 1) & 0b0000111111111111) * potScale;

	fprintf (cs, "1");
	fflush (cs);
}

int main (int argc, char **argv)
{
	int spiFile1;
	int spiFile2;
        struct gpioInfo cs[3] = {{27}, {10}, {31}};
        struct gpioInfo controlValve[6] = {{65}, {66}, {67}, {68}, {69}, {12}};

	FILE *pidFile;
	char *line = NULL;
	char *tok;
	float TOP_UP;
	float TOP_DOWN;
	float BOTTOM_UP;
	float BOTTOM_DOWN;
	float P = 15.0;
	float I = 0.0;
	float D = 0.0;
	float pScale[6] = {0};

        FILE *file;
	char filePath[255];
	float fBuffer;
	int stepNum;
	int timeStep;
	int loopEnd;
	int loopStart;
	float setLength[6] = {0, 0, 0, 0, 0, 0};

        FILE *outFile;
	size_t len;
	time_t timep;
	struct tm tm;
	float lengthBuffer[6];

	struct timeval startTime;
	int curTime = 0;
	int lastTime;
	int progTime;

	float error[6] = {0};
	float lastError[6];
	float intError[6] = {0};
	float dError;

	int i, j, k, l;

	if (setjmp (buf))
		goto shutdown;

	signal (SIGINT, signalCatcher);

	if (argc != 2) {
		fprintf (stderr, "Expects one input, the name of the file to run.\n");
		return 1;
	}

	len = strlen (argv[1]);
	if (!(argv[1][len - 3] == 'b' && argv[1][len - 2] == 'i' && argv[1][len - 1] == 'n')) {
		fprintf (stderr, "Expects a .bin file.\n");
		return 1;
	}

	sprintf (filePath, "binFiles/%s", argv[1]);
        if (!(file = fopen (filePath, "r"))) {
                fprintf (stderr, "Failed to open input file: %s\n", argv[1]);
                return 1;
        }

        fread (&fBuffer, sizeof(float), 1, file);
	stepNum = (int)fBuffer;
        fread (&fBuffer, sizeof(float), 1, file);
	timeStep = (int)fBuffer * 1000;
        fread (&fBuffer, sizeof(float), 1, file);
	loopEnd = (int)fBuffer + 1;
        fread (&fBuffer, sizeof(float), 1, file);
	loopStart = (int)fBuffer;

	printf ("stepNum: %d\ntimeStep: %d\nloopEnd: %d\nloopStart: %d\n", stepNum, timeStep, loopEnd, loopStart);

	argv[1][len - 4] = '\0';

	if (!(pidFile = fopen ("pidFile.txt", "r"))) {
		fprintf (stderr, "Filed to open PID file\n");
		return 1;
	}

	while (1) {
		if (getline (&line, &len, pidFile) == -1) {
			fprintf (stderr, "Could not find %s in pidFile.txt.\n", argv[1]);
			return 1;
		}
		tok = strtok(line, "\t");
		if (strcmp (argv[1], tok) == 0) {
			tok = strtok(NULL, "\t");
			P = atof (tok);
			tok = strtok(NULL, "\t");
			I = atof (tok);
			tok = strtok(NULL, "\t");
			D = atof (tok);
			tok = strtok(NULL, "\t");
			TOP_UP = atof (tok);
			tok = strtok(NULL, "\t");
			TOP_DOWN = atof (tok);
			tok = strtok(NULL, "\t");
			BOTTOM_UP = atof (tok);
			tok = strtok(NULL, "\t");
			BOTTOM_DOWN = atof (tok);
			for (i = 0; i < 6; i++) {
				tok = strtok(NULL, "\t");
				pScale[i] = atof (tok);
			}
			break;
		}
	}
	free (line);
	fclose (pidFile);

	printf ("\nP: %f\nI: %f\nD: %f\n", P, I, D);
	printf ("\ntopUp: %f\ntopDown: %f\nbottomUp: %f\nbottomDown: %f\n", TOP_UP, TOP_DOWN, BOTTOM_UP, BOTTOM_DOWN);
	printf ("pScale: ");
	for (i = 0; i < 5; i++)
		printf ("%f, ", pScale[i]);
	printf ("%f\n", pScale[5]);

	pruInit ();

	prussdrv_map_prumem (PRUSS0_PRU1_DATARAM, &pruDataMem);
	pruDataMem_int = (unsigned int*) pruDataMem;

        for (i = 0; i < 12; i++)
		pruDataMem_int[i] = 0;

        for (i = 0; i < 6; i++)
                gpioOutputInit (&controlValve[i], "0");

	if (prussdrv_exec_program (1, "./stepper.bin")) {
		fprintf (stderr, "prussdrv_exec_program (1, './stepper.bin') failed\n");
		return 1;
	}

	spiInit ("/dev/spidev1.0", &spiFile1);
	spiInit ("/dev/spidev2.0", &spiFile2);

	for (i = 0; i < 3; i++)
                gpioOutputInit (&cs[i], "1");

	for (i = 0; i < 12; i++)
                pruDataMem_int[i] = 20;

        for (i = 0; i != 0b111111; ) {
                for (j = 0, k = 0, l = 0; j < 3; j += 1, k += 2, l += 4) {
                        float lengthSensor[2];

                        spiRead (spiFile1, spiFile2, cs[j].file, lengthSensor);

                        if (lengthSensor[0] > 19) {
                                pruDataMem_int[motor[l]] = 0;
                                pruDataMem_int[motor[l + 1]] = 0;
                                i |= 1 << k;
                        }

                        if (lengthSensor[1] > 19) {
                                pruDataMem_int[motor[l + 2]] = 0;
                                pruDataMem_int[motor[l + 3]] = 0;
                                i |= 1 << (k + 1);
                        }
                }
        }

        for (i = 0; i < 12; i++)
                pruDataMem_int[i] = 0;

        puts ("JUMPPED");

        sleep (1);

	timep = time (NULL);
	tm = *localtime (&timep);
	sprintf (filePath, "csvFiles/%s_%d-%d-%d_%d-%d-%d.csv", argv[1], tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (!(outFile = fopen (filePath, "w"))) {
		fprintf (stderr, "Filed to open output file: %s\n", argv[1]);
		return 1;
	}

	printf ("\nStarted at: %02d:%02d:%02d\n\n", tm.tm_hour, tm.tm_min, tm.tm_sec);

	fprintf (outFile, "t, c1, c2, c3, c4, c5, c6, ");
	fprintf (outFile, "stepNum, %d, timeStep, %d, loopEnd, %d, loopStart, %d, ", stepNum, timeStep, loopEnd, loopStart);
	fprintf (outFile, "P, %f, I, %f, D, %f, ", P, I, D);
	fprintf (outFile, "topUp, %f, topDown, %f, bottomUp, %f, bottomDown, %f\n", TOP_UP, TOP_DOWN, BOTTOM_UP, BOTTOM_DOWN);
	fprintf (outFile, "pScale, ");
	for (i = 0; i < 6; i++)
		fprintf (outFile, "%f, ", pScale[i]);

	gettimeofday (&startTime, NULL);
	progTime = timeStep;
	i = 0;
LOOP:
	for (; i < loopEnd; ) {
		struct timeval loopTime;

		lastTime = curTime;
		gettimeofday (&loopTime, NULL);
		curTime = ((loopTime.tv_sec - startTime.tv_sec) * 1000000) + loopTime.tv_usec - startTime.tv_usec;
		if (curTime > progTime) {
			for (j = 0; j < 6; j++)
				fread (&setLength[j], sizeof(float), 1, file);

			progTime += timeStep;
			i++;

			fprintf (outFile, "%d, ", curTime);
			for (j = 0; j < 5; j++)
				fprintf (outFile, "%f, ", lengthBuffer[j] - 17.0);
			fprintf (outFile, "%f\n", lengthBuffer[5] - 17.0);
		}

		for (j = 0, k = 0, l = 0; j < 3; j += 1, k += 2, l += 4) {
			float lengthSensor[2];
			float PIDOut;
			float dt;

			spiRead (spiFile1, spiFile2, cs[j].file, lengthSensor);

			dt = (float)(curTime - lastTime);

			lastError[k] = error[k];
			error[k] = setLength[k] - lengthSensor[0] + 17.0;
			intError[k] += error[k] * dt;
			dError = (error[k] - lastError[k]) / dt;
			
			if (lengthSensor[0] > 46.0 && dError < maxVelocity) {
				fprintf (stderr, "Length sensor %d is %f, past the 46 inch limit\n", k + 1, lengthSensor[0]);
				goto shutdown;
			} else if (lengthSensor[0] < 18.0 && dError < maxVelocity) {
				fprintf (stderr, "Length sensor %d is %f, below the 18 inch limit\n", k + 1, lengthSensor[0]);
				goto shutdown;
			}
			
			PIDOut = (P * pScale[k] * error[k]) + (I * intError[k]) - (D * dError);

			if (PIDOut > 0) {
				pruDataMem_int[motor[l]] = (int)((PIDOut * TOP_UP) + 0.5);
				pruDataMem_int[motor[l + 1]] = (int)((PIDOut * BOTTOM_UP) + 0.5);
			} else {
				pruDataMem_int[motor[l]] = (int)((PIDOut * TOP_DOWN) + 0.5);
				pruDataMem_int[motor[l + 1]] = (int)((PIDOut * BOTTOM_DOWN) + 0.5);
			}

			lastError[k + 1] = error[k + 1];
			error[k + 1] = setLength[k + 1] - lengthSensor[1] + 17.0;
			intError[k + 1] += error[k + 1] * dt;
			dError = (error[k + 1] - lastError[k + 1]) / dt;
			
			if (lengthSensor[1] > 46.0 && dError < maxVelocity) {
				fprintf (stderr, "Length sensor %d is %f, past the 46 inch limit\n", k + 2, lengthSensor[1]);
				goto shutdown;
			} else if (lengthSensor[1] < 18.0 && dError < maxVelocity) {
				fprintf (stderr, "Length sensor %d is %f, below the 18 inch limit\n", k + 2, lengthSensor[1]);
				goto shutdown;
			}
			
			PIDOut = (P * pScale[k + 1] * error[k + 1]) + (I * intError[k + 1]) - (D * dError);

			if (PIDOut > 0) {
				pruDataMem_int[motor[l + 2]] = (int)((PIDOut * TOP_UP) + 0.5);
				pruDataMem_int[motor[l + 3]] = (int)((PIDOut * BOTTOM_UP) + 0.5);
			} else {
				pruDataMem_int[motor[l + 2]] = (int)((PIDOut * TOP_DOWN) + 0.5);
				pruDataMem_int[motor[l + 3]] = (int)((PIDOut * BOTTOM_DOWN) + 0.5);
			}

			lengthBuffer[k] = lengthSensor[0];
			lengthBuffer[k + 1] = lengthSensor[1];
		}
        }
	fseek (file, ((loopStart * 6) + 4) * sizeof(float), SEEK_SET);
	gettimeofday (&startTime, NULL);
	i = loopStart;
	curTime = 0;
	progTime = timeStep;
	for (j = 0; j < 6; j++)
		error[j] = 0;

	goto LOOP;

shutdown:

        for (i = 0; i < 12; i++)
		pruDataMem_int[i] = 0;

	fclose (outFile);

	sleep (3);

        for (i = 0; i < 6; i++)
                gpioOutputAbscond (&controlValve[i], "0");

	pruAbscond ();

	close (spiFile1);
	close (spiFile2);

        for (i = 0; i < 3; i++)
                gpioOutputAbscond (&cs[i], "1");

        fclose (file);

	return 0;
}

