#include <stdio.h>

#include <signal.h>
#include <setjmp.h>

#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#define PRESSURE_SCALE 0.0022663493 //1.6 * 23.206 / 16383
#define FORCE_SCALE 1.164153357e-4 //(0.5 * 3.3 / 128) / (2^23 - 1) / (2e-3 * 3.3) * 500

static int spiBits = 8;
static int spiSpeed = 20000;

static jmp_buf buf;

void signalCatcher (int null)
{
	longjmp (buf, 1);
}

void spiInit (char *file, int *fd)
{
	*fd = open (file, O_RDWR);
	ioctl (*fd, SPI_IOC_WR_BITS_PER_WORD, &spiBits);
	ioctl (*fd, SPI_IOC_RD_BITS_PER_WORD, &spiBits);
	ioctl (*fd, SPI_IOC_WR_MAX_SPEED_HZ, &spiSpeed);
	ioctl (*fd, SPI_IOC_RD_MAX_SPEED_HZ, &spiSpeed);
}

float spiRead16 (int fd)
{
	uint8_t tx[2] = {0, 0};
	uint8_t rx[2];
	struct spi_ioc_transfer tr;

	tr.tx_buf = (unsigned long)tx;
	tr.rx_buf = (unsigned long)rx;
	tr.len = 2;
	tr.speed_hz = spiSpeed;
	tr.bits_per_word = spiBits;

	ioctl (fd, SPI_IOC_MESSAGE (1), &tr);

        return rx[1] << 8 | rx[2];
}

float spiRead24 (int fd)
{
	uint8_t tx[3] = {0, 0, 0};
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

int inetServerInit (void)
{
	int sock;
	int returnSock;
	struct sockaddr_in server;

	if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf (stderr, "socket(AF_INET, SOCK_STREAM, 0) failed\n");
		return -1;
	}

	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_family = AF_INET;
	server.sin_port = htons (8888);

	if (bind (sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
		fprintf (stderr, "bind() failed\n");
		return -1;
	}

	listen (sock, 3);

	if ((returnSock = accept (sock, NULL, NULL)) < 0) {
		fprintf (stderr, "accept() failed\n");
		return -1;
	}

	return returnSock;
}

int main (int argc, char **argv)
{
	int spiFile1;
	int spiFile2;

	int sock;
	char buffer[255];

	int temp;
	float pressure;
	float force;

	struct timespec waitTime = {0, 100000000};

	if (setjmp (buf))
		goto shutdown;

	signal (SIGINT, signalCatcher);

	spiInit ("/dev/spidev1.0", &spiFile1);
	spiInit ("/dev/spidev2.0", &spiFile2);

	sock = inetServerInit ();

	while (1) {
		nanosleep (&waitTime, NULL);

		temp = spiRead24 (spiFile2);
		if (temp & (1 << 23))
			temp |= 0xFF000000;
		force = (float)temp * FORCE_SCALE;
		sprintf (buffer, "%f", force);
		send (sock, buffer, 255, 0);

		temp = spiRead16 (spiFile1);
		if (temp & (1 << 15))
			temp |= 0xFFFF0000;
		pressure = (float)temp * PRESSURE_SCALE;
		sprintf (buffer, "%f", pressure);
		send (sock, buffer, 255, 0);
	}

shutdown:

	close (spiFile1);
	close (spiFile2);

	return 0;
}

