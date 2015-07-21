#include <stdio.h>

#include <signal.h>
#include <setjmp.h>

#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>

static int spiBits = 8;
static int spiSpeed = 500000;

static float potScale = 0.0120147945;

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

	int i;

	if (setjmp (buf))
		goto shutdown;

	signal (SIGINT, signalCatcher);

	if (argc != 1)
		puts ("Parameters are ignored.");

	spiInit ("/dev/spidev1.0", &spiFile1);
	spiInit ("/dev/spidev2.0", &spiFile2);

	for (i = 0; i < 3; i++)
		gpioOutputInit (&cs[i], "1");
				
	while (1) {
		for (i = 0; i < 3; i++) {
			float lengthSensor[2];
			spiRead (spiFile1, spiFile2, cs[i].file, lengthSensor);
			printf ("%f, %f, ", lengthSensor[0], lengthSensor[1]);
		}
        puts ("");
	}

shutdown:

	close (spiFile1);
	close (spiFile2);

    for (i = 0; i < 3; i++)
		gpioOutputAbscond (&cs[i], "1");

	return 0;
}

