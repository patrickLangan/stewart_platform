#include <stdio.h>

#include <signal.h>
#include <setjmp.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>

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

int gpioOutputInit (struct gpioInfo *gpio)
{
        char gpioPath[30];

        sprintf (gpioPath, "/sys/class/gpio/gpio%d/value", gpio->pin);
        gpio->file = fopen (gpioPath, "w");
        fprintf (gpio->file, "0");
        fflush (gpio->file);

        return 0;
}

int gpioOutputAbscond (struct gpioInfo *gpio)
{
        fprintf (gpio->file, "0");
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

int spiRead (int fd, int cs)
{
	uint8_t tx[2] = {1, 1};
	uint8_t rx[2];
	struct spi_ioc_transfer tr;

	tr.tx_buf = (unsigned long)tx;
	tr.rx_buf = (unsigned long)rx;
	tr.len = 2;
	tr.speed_hz = spiSpeed;
	tr.bits_per_word = spiBits;

	ioctl (fd, SPI_IOC_MESSAGE (1), &tr);

	return (rx[0] << 8) | rx[1];
}

int main (int argc, char **argv)
{
	FILE *joystickFile;

	int spiFile1;
	int spiFile2;
        struct gpioInfo cs[3] = {{27}, {30}, {31}};
	int lengthSensor[6];

        struct gpioInfo controlValve[6] = {{65}, {66}, {67}, {68}, {69}, {78}};

	int i;

	if (setjmp (buf))
		goto shutdown;

	signal (SIGINT, signalCatcher);

	joystickFile = fopen ("/sys/devices/ocp.2/helper.13/AIN1", "r");

	pruInit ();

	prussdrv_map_prumem (PRUSS0_PRU1_DATARAM, &pruDataMem);
	pruDataMem_int = (unsigned int*) pruDataMem;

	if (prussdrv_exec_program (1, "./stepper.bin")) {
		fprintf (stderr, "prussdrv_exec_program (1, './stepper.bin') failed\n");
		return 1;
	}

	spiInit ("/dev/spidev1.0", &spiFile1);
	spiInit ("/dev/spidev2.0", &spiFile2);

	for (i = 0; i < 3; i++)
                gpioOutputInit (&cs[i]);

        for (i = 0; i < 6; i++)
                gpioOutputInit (&controlValve[i]);

shutdown:

	fclose (joystickFile);

	pruAbscond ();

	close (spiFile1);
	close (spiFile2);

        for (i = 0; i < 3; i++)
                gpioOutputAbscond (&cs[i]);

        for (i = 0; i < 6; i++)
                gpioOutputAbscond (&controlValve[i]);

	return 0;
}

