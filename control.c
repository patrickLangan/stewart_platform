#include <stdio.h>

#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>

static int spiBits = 8;
static int spiSpeed = 500000;

void spiInit (char *file, int *fd)
{
	*fd = open (file, O_RDWR);
	ioctl (*fd, SPI_IOC_WR_BITS_PER_WORD, &spiBits);
	ioctl (*fd, SPI_IOC_RD_BITS_PER_WORD, &spiBits);
	ioctl (*fd, SPI_IOC_WR_MAX_SPEED_HZ, &spiSpeed);
	ioctl (*fd, SPI_IOC_RD_MAX_SPEED_HZ, &spiSpeed);
}

float spiRead (int fd)
{
	int temp;

	uint8_t tx[3] = {1, 1, 1};
	uint8_t rx[3];
	struct spi_ioc_transfer tr;

	tr.tx_buf = (unsigned long)tx;
	tr.rx_buf = (unsigned long)rx;
	tr.len = 3;
	tr.speed_hz = spiSpeed;
	tr.bits_per_word = spiBits;

	ioctl (fd, SPI_IOC_MESSAGE (1), &tr);

        temp = (rx[0] << 16) | (rx[1] << 8) | rx[2];

	if (temp & (1 << 23))
		temp |= 0xFF000000;

	return (float)temp * 1.164153357e-4; //(0.5 * 3.3 / 128) / (2^23 - 1) / (2e-3 * 3.3) * 500
}

int main (int argc, char **argv)
{
	int spiFile;
	float force;
	struct timespec waitTime = {0, 100000000};

	spiInit ("/dev/spidev1.0", &spiFile);

	while (1) {
		nanosleep (&waitTime, NULL);
		force = spiRead (spiFile);
		printf ("%f\n", force);
	}

	close (spiFile);

	return 0;
}
