#include <stdio.h>

#include <signal.h>
#include <setjmp.h>

#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#define PRESSURE_SCALE 0.0091558323
#define LENGTH_SCALE 3.906379093e-4
#define FORCE_SCALE 1.164153357e-4 //(0.5 * 3.3 / 128) / (2^23 - 1) / (2e-3 * 3.3) * 500

static int spiBits = 8;
static int spiSpeed = 20000;

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

void spiInit (char *file, int *fd)
{
	*fd = open (file, O_RDWR);
	ioctl (*fd, SPI_IOC_WR_BITS_PER_WORD, &spiBits);
	ioctl (*fd, SPI_IOC_RD_BITS_PER_WORD, &spiBits);
	ioctl (*fd, SPI_IOC_WR_MAX_SPEED_HZ, &spiSpeed);
	ioctl (*fd, SPI_IOC_RD_MAX_SPEED_HZ, &spiSpeed);
}

float spiRead24 (int fd)
{
	uint8_t tx[3] = {1, 1, 1};
	uint8_t rx[3];
	struct spi_ioc_transfer tr;

	tr.tx_buf = (unsigned long)tx;
	tr.rx_buf = (unsigned long)rx;
	tr.len = 3;
	tr.speed_hz = spiSpeed;
	tr.bits_per_word = spiBits;

	ioctl (fd, SPI_IOC_MESSAGE (1), &tr);

        return rx[0] << 16 | rx[1] << 8 | rx[2];
}

/* Source: http://aggregate.org/MAGIC/#Bit%20Reversal */
unsigned int bitReverse (unsigned int x)
{
	x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
	x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
	x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
	x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));

	return((x >> 16) | (x << 16));
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

/*
	spiInit ("/dev/spidev1.0", &spiFile1);
	spiInit ("/dev/spidev2.0", &spiFile2);
*/

	pressHandle1 = i2c_open (1, 0x28);
	pressHandle2 = i2c_open (2, 0x28);

	while (1) {
		printf ("%d\n", pruDataMem1_int[0]);
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

/*
	while (1) {
		nanosleep (&waitTime, NULL);

		temp = spiRead24 (spiFile1);
		temp = bitReverse (temp);
		length1 = (float)(temp >> 8) * LENGTH_SCALE;

		temp = spiRead24 (spiFile2);
		temp = bitReverse (temp);
		length2 = (float)(temp >> 8) * LENGTH_SCALE;

		printf ("%f, %f\n", length1, length2);
	}
*/

/*
	while (1) {
		nanosleep (&waitTime, NULL);
		temp = spiRead24 (spiFile);
		if (temp & (1 << 23))
			temp |= 0xFF000000;
		force = (float)temp * FORCE_SCALE;
		printf ("%f\n", force);
	}
*/

shutdown:

	pruTerminate ();

/*
	close (spiFile1);
	close (spiFile2);
*/

	return 0;
}
