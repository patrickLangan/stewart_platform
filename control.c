#include <stdio.h>

#include <signal.h>
#include <setjmp.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>

static double P = 10.0;

static int spiBits = 8;
static int spiSpeed = 500000;

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

double gettimefromfunction (struct timeval startTime)
{
	struct timeval curTime;

	gettimeofday (&curTime, NULL);

	return ((double)(curTime.tv_sec - startTime.tv_sec) * 1e3) + ((double)(curTime.tv_usec - startTime.tv_usec) * 1e-3);
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

float spiRead (int fd, FILE *cs)
{
	uint8_t tx[2] = {1, 1};
	uint8_t rx[2];
	struct spi_ioc_transfer tr;

	fprintf (cs, "0");
	fflush (cs);

	tr.tx_buf = (unsigned long)tx;
	tr.rx_buf = (unsigned long)rx;
	tr.len = 2;
	tr.speed_hz = spiSpeed;
	tr.bits_per_word = spiBits;

	ioctl (fd, SPI_IOC_MESSAGE (1), &tr);

	fprintf (cs, "1");
	fflush (cs);

        return (float)((((rx[0] << 8) | rx[1]) >> 1) & 0b0000111111111111) * 0.0120147945;
}

int main (int argc, char **argv)
{
	int spiFile1;
	int spiFile2;
        struct gpioInfo cs[3] = {{27}, {10}, {31}};
	double lengthSensor[6];

        struct gpioInfo controlValve[6] = {{65}, {66}, {67}, {68}, {69}, {12}};

        FILE *file;
        double stepNum;
        double timeStep;
        double loopAddr;
        double setLength;

	struct timeval startTime;
	double time;

	int i, j, k;

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

        fread (&stepNum, sizeof(double), 1, file);
        fread (&timeStep, sizeof(double), 1, file);
        fread (&loopAddr, sizeof(double), 1, file);

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
		pruDataMem_int[i] = 25;

	for (i = 0; i != 0b111111; ) {
                lengthSensor[0] = spiRead (spiFile1, cs[0].file);
		if (lengthSensor[0] > 17.25) {
			pruDataMem_int[6] = 0;
			pruDataMem_int[7] = 0;
			i |= 0b1;
		}
                lengthSensor[1] = spiRead (spiFile1, cs[1].file);
		if (lengthSensor[1] > 17.25) {
			pruDataMem_int[2] = 0;
			pruDataMem_int[3] = 0;
			i |= 0b10;
		}
                lengthSensor[2] = spiRead (spiFile1, cs[2].file);
		if (lengthSensor[2] > 17.25) {
			i |= 0b100;
			pruDataMem_int[10] = 0;
			pruDataMem_int[11] = 0;
		}
                lengthSensor[3] = spiRead (spiFile2, cs[0].file);
		if (lengthSensor[3] > 17.25) {
			i |= 0b1000;
			pruDataMem_int[0] = 0;
			pruDataMem_int[1] = 0;
		}
                lengthSensor[4] = spiRead (spiFile2, cs[1].file);
		if (lengthSensor[4] > 17.25) {
			i |= 0b10000;
			pruDataMem_int[4] = 0;
			pruDataMem_int[5] = 0;
		}
                lengthSensor[5] = spiRead (spiFile2, cs[2].file);
		if (lengthSensor[5] > 17.25) {
			i |= 0b100000;
			pruDataMem_int[8] = 0;
			pruDataMem_int[9] = 0;
		}
	}

        for (i = 0; i < 12; i++)
		pruDataMem_int[i] = 0;

	puts ("JUMPPED");

	//goto shutdown;

	sleep (1);

	gettimeofday (&startTime, NULL);

	i = 0;
LOOP:
	for (; i <= stepNum; i++) {
		double PIDOut;

		//Cylinder 1
		fread (&setLength, sizeof(double), 1, file);
                lengthSensor[0] = spiRead (spiFile1, cs[0].file);
		PIDOut = P * (setLength - lengthSensor[0] + 17.0);
		pruDataMem_int[6] = (int)PIDOut;
		pruDataMem_int[7] = (int)PIDOut;
		printf ("%lf, %lf, %lf, %d, %d, %d, %d, ", setLength, lengthSensor[0], PIDOut, pruDataMem_int[6], pruDataMem_int[7], pruDataMem_int[97], pruDataMem_int[98]);

		//Cylinder 2
		fread (&setLength, sizeof(double), 1, file);
                lengthSensor[5] = spiRead (spiFile2, cs[2].file);
		PIDOut = P * (setLength - lengthSensor[5] + 17.0);
		pruDataMem_int[8] = (int)PIDOut;
		pruDataMem_int[9] = (int)PIDOut;
		printf ("%lf, %lf, %lf, %d, %d, %d, %d, ", setLength, lengthSensor[5], PIDOut, pruDataMem_int[8], pruDataMem_int[9], pruDataMem_int[99], pruDataMem_int[100]);

		//Cylinder 3
		fread (&setLength, sizeof(double), 1, file);
                lengthSensor[2] = spiRead (spiFile1, cs[2].file);
		PIDOut = P * (setLength - lengthSensor[2] + 17.0);
		pruDataMem_int[10] = (int)PIDOut;
		pruDataMem_int[11] = (int)PIDOut;
		printf ("%lf, %lf, %lf, %d, %d, %d, %d, ", setLength, lengthSensor[2], PIDOut, pruDataMem_int[10], pruDataMem_int[11], pruDataMem_int[101], pruDataMem_int[102]);

		//Cylinder 4
		fread (&setLength, sizeof(double), 1, file);
                lengthSensor[4] = spiRead (spiFile2, cs[1].file);
		PIDOut = P * (setLength - lengthSensor[4] + 17.0);
		pruDataMem_int[4] = (int)PIDOut;
		pruDataMem_int[5] = (int)PIDOut;
		printf ("%lf, %lf, %lf, %d, %d, %d, %d, ", setLength, lengthSensor[4], PIDOut, pruDataMem_int[4], pruDataMem_int[5], pruDataMem_int[95], pruDataMem_int[96]);

		//Cylinder 5
		fread (&setLength, sizeof(double), 1, file);
                lengthSensor[1] = spiRead (spiFile1, cs[1].file);
		PIDOut = P * (setLength - lengthSensor[1] + 17.0);
		pruDataMem_int[2] = (int)PIDOut;
		pruDataMem_int[3] = (int)PIDOut;
		printf ("%lf, %lf, %lf, %d, %d, %d, %d, ", setLength, lengthSensor[1], PIDOut, pruDataMem_int[2], pruDataMem_int[3], pruDataMem_int[93], pruDataMem_int[94]);

		//Cylinder 6
		fread (&setLength, sizeof(double), 1, file);
                lengthSensor[3] = spiRead (spiFile2, cs[0].file);
		PIDOut = P * (setLength - lengthSensor[3] + 17.0);
		pruDataMem_int[0] = (int)PIDOut;
		pruDataMem_int[1] = (int)PIDOut;
		printf ("%lf, %lf, %lf, %d, %d, %d, %d, ", setLength, lengthSensor[3], PIDOut, pruDataMem_int[0], pruDataMem_int[1], pruDataMem_int[91], pruDataMem_int[92]);

		do {
			time = gettimefromfunction (startTime);
		} while (time < i * timeStep);

		printf ("%lf\n", time);
        }
	fseek (file, (((int)loopAddr * 6) + 3) * sizeof(double), SEEK_SET);
	i = (int)loopAddr;
	goto LOOP;

shutdown:

        for (i = 0; i < 12; i++)
		pruDataMem_int[i] = 0;

	sleep (5);

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

