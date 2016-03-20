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

int udpInit (const unsigned int port)
{
        struct sockaddr_in localSock = {
                .sin_family = AF_INET
        };
        int sock;

        if ((sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
                fprintf (stderr, "socket() failed\n");
                return -1;
        }

        localSock.sin_port = htons (port);
        localSock.sin_addr.s_addr = htonl (INADDR_ANY);

        if (bind (sock, (struct sockaddr *)&localSock, sizeof(localSock)) < 0) {
                fprintf (stderr, "bind() failed\n");
                return -1;
        }

        return sock;
}

int main (int argc, char **argv)
{
        struct sockaddr_in client = {
                .sin_family = AF_INET
        };
        char buffer[255] = {'\0'};
        int sock;

        if (setjmp (buf))
                goto shutdown;

        signal (SIGINT, signalCatcher);

        sock = udpInit (1680);

        client.sin_port = htons (1681);
        inet_aton ("192.168.0.11", &client.sin_addr);

        while (1) {
                sprintf (buffer, "Testing bbb1 UDP connection.\0");
                sendto (sock, buffer, 255, 0, (struct sockaddr *)&client, sizeof(client));
                usleep (5000);
        }

shutdown:
        close (sock);

        return 0;
}

