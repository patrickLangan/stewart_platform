#include <stdio.h>

#include <signal.h>
#include <setjmp.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>

#include <math.h>

#define TOP_UP 1.0
#define TOP_DOWN 1.0
#define BOTTOM_UP 1.0
#define BOTTOM_DOWN 0.5

static float P[6] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
static float PScale = 10.0;

static float D[6] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
static float DScale = 1.0e6;

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

        FILE *file;
	float fBuffer;
	int stepNum;
	int timeStep;
	int loopAddr;
	float setLength[6] = {0, 0, 0, 0, 0, 0};

	struct timeval startTime;
	int time = 0;
	int lastTime;
	int progTime;

	float error[6] = {0};
	float lastError[6];

	int i, j, k, l;

	float lb[6];

	if (setjmp (buf))
		goto shutdown;

	signal (SIGINT, signalCatcher);

        if (
                ((argc < 2) ? fprintf (stderr, "You need to give a file name\n") : 0) ||
                ((argc > 2) ? fprintf (stderr, "Too many arguments\n") : 0)
        )
                return 1;

        if (!(file = fopen (argv[1], "r"))) {
                fprintf (stderr, "Failed to open file\n");
                return 1;
        }

        fread (&fBuffer, sizeof(float), 1, file);
	stepNum = (int)fBuffer;
        fread (&fBuffer, sizeof(float), 1, file);
	timeStep = (int)fBuffer * 1000;
        fread (&fBuffer, sizeof(float), 1, file);
	loopAddr = (int)fBuffer;

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
		pruDataMem_int[i] = 15;

	for (i = 0; i != 0b111111; ) {
		for (j = 0, k = 0, l = 0; j < 3; j += 1, k += 2, l += 4) {
			float lengthSensor[2];

			spiRead (spiFile1, spiFile2, cs[j].file, lengthSensor);

			if (lengthSensor[0] > 18) {
				if (lengthSensor[0] > 45)
					goto shutdown;
				pruDataMem_int[motor[l]] = 0;
				pruDataMem_int[motor[l + 1]] = 0;
				i |= 1 << k;
			}

			if (lengthSensor[1] > 18) {
				if (lengthSensor[1] > 45)
					goto shutdown;
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

	gettimeofday (&startTime, NULL);
	progTime = 0;
	i = 0;
LOOP:
	for (; i <= stepNum; ) {
		struct timeval curTime;

		lastTime = time;
		gettimeofday (&curTime, NULL);
		time = ((curTime.tv_sec - startTime.tv_sec) * 1000000) + curTime.tv_usec - startTime.tv_usec;
		if (time > progTime) {
			for (j = 0; j < 6; j++) {
				fread (&setLength[j], sizeof(float), 1, file);
				if (fabs (setLength[j]) > 28.0) {
					fprintf (stderr, "Set length is %f, past the 28 inch limit\n", setLength[j]);
					goto shutdown;
				}
			}
			progTime += timeStep;
			i++;
			printf ("%d, %f, %f, %f, %f, %f, %f\n", time, lb[0], lb[1], lb[2], lb[3], lb[4], lb[5]);
		}

		for (j = 0, k = 0, l = 0; j < 3; j += 1, k += 2, l += 4) {
			float lengthSensor[2];
			float PIDOut;

			spiRead (spiFile1, spiFile2, cs[j].file, lengthSensor);
			if (lengthSensor[0] > 45.0) {
				fprintf (stderr, "Length sensor is %f, past the 28 inch limit\n", lengthSensor[0]);
				goto shutdown;
			}
			if (lengthSensor[1] > 45.0) {
				fprintf (stderr, "Length sensor is %f, past the 28 inch limit\n", lengthSensor[1]);
				goto shutdown;
			}

			lastError[k] = error[k];
			error[k] = setLength[k] - lengthSensor[0] + 17.0;
			PIDOut = (P[k] * PScale * error[k]) + (D[k] * DScale * ((error[k] - lastError[k]) / ((float)(time - lastTime))));

			if (PIDOut > 0) {
				pruDataMem_int[motor[l]] = (int)((PIDOut * TOP_UP) + 0.5);
				pruDataMem_int[motor[l + 1]] = (int)((PIDOut * BOTTOM_UP) + 0.5);
			} else {
				pruDataMem_int[motor[l]] = (int)((PIDOut * TOP_DOWN) + 0.5);
				pruDataMem_int[motor[l + 1]] = (int)((PIDOut * BOTTOM_DOWN) + 0.5);
			}

			lastError[k + 1] = error[k + 1];
			error[k + 1] = setLength[k + 1] - lengthSensor[1] + 17.0;
			PIDOut = (P[k + 1] * PScale * error[k + 1]) + (D[k + 1] * DScale * ((error[k + 1] - lastError[k + 1]) / ((float)(time - lastTime))));

			if (PIDOut > 0) {
				pruDataMem_int[motor[l + 2]] = (int)((PIDOut * TOP_UP) + 0.5);
				pruDataMem_int[motor[l + 3]] = (int)((PIDOut * BOTTOM_UP) + 0.5);
			} else {
				pruDataMem_int[motor[l + 2]] = (int)((PIDOut * TOP_DOWN) + 0.5);
				pruDataMem_int[motor[l + 3]] = (int)((PIDOut * BOTTOM_DOWN) + 0.5);
			}

			lb[k] = lengthSensor[0];
			lb[k + 1] = lengthSensor[1];
		}
        }

	fseek (file, ((loopAddr * 6) + 3) * sizeof(float), SEEK_SET);
	gettimeofday (&startTime, NULL);
	progTime = 0;
	time = 0;
	i = loopAddr;

	goto LOOP;

shutdown:

        for (i = 0; i < 12; i++)
		pruDataMem_int[i] = 0;

	for (i = 0; i < 12; puts ("")) {
                if ((abs (pruDataMem_int[91 + i]) >> 1) == 0)
                        i++;
                for (j = 0; j < 12; j++)
                        printf ("%d, ", pruDataMem_int[91 + j]);
        }

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

