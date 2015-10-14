#include <stdio.h>

#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>

static int spiBits = 8;
static int spiSpeed = 20000;

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
	int spiFile;
	int temp;
	float force;
	float length;
	struct timespec waitTime = {0, 100000000};

	spiInit ("/dev/spidev1.0", &spiFile);

	while (1) {
		nanosleep (&waitTime, NULL);
		temp = spiRead24 (spiFile);
		temp = bitReverse (temp);
		length = (float)(temp >> 8) * 3.906379093e-4;
		printf ("%f\n", length);
	}

/*
	while (1) {
		nanosleep (&waitTime, NULL);
		temp = spiRead24 (spiFile);
		if (temp & (1 << 23))
			temp |= 0xFF000000;
		force = (float)temp * 1.164153357e-4; //(0.5 * 3.3 / 128) / (2^23 - 1) / (2e-3 * 3.3) * 500
		printf ("%f\n", force);
	}
*/

	close (spiFile);

	return 0;
}
