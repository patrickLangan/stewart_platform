#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <signal.h>
#include <setjmp.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#define PRESSURE_SCALE 0.0091558323
#define LENGTH_SCALE 3.123433875e-4
#define LENGTH_ZERO 993

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
	char buffer[255] = {'\0'};
	int sock;

        int temp;
        float length;
        float pressure1;
        float pressure2;

	int lengthHandle;
        int pressHandle1;
        int pressHandle2;

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

	pruDataMem1_int[0] = 0;
	pruDataMem1_int[1] = 0;

        pressHandle1 = i2c_open (1, 0x28);
        pressHandle2 = i2c_open (2, 0x28);

	sock = udpInit (1681);

	//Test stepper motors / directional control valves
	if (argc == 2 && argv[1][0] == 'e') {
		pruDataMem1_int[0] = 400;
		pruDataMem1_int[1] = 400;
		sleep (4);
		pruDataMem1_int[0] = 0;
		pruDataMem1_int[1] = 0;
		sleep (4);
		goto shutdown;
	}
	if (argc == 2 && argv[1][0] == 'r') {
		pruDataMem1_int[0] = -400;
		pruDataMem1_int[1] = -400;
		sleep (4);
		pruDataMem1_int[0] = 0;
		pruDataMem1_int[1] = 0;
		sleep (4);
		goto shutdown;
	}

	//Test length sensor
	if (argc == 2 && argv[1][0] == 'l') {
		while (1) {
			temp = pruDataMem0_int[0];
			length = (float)(temp - LENGTH_ZERO) * LENGTH_SCALE;
			printf ("%f\n", length / 0.0254);

			usleep (5000);
		}
		goto shutdown;
	}

	//Test pressure sensors
	if (argc == 2 && argv[1][0] == 'p') {
		while (1) {
			temp = i2cRead (pressHandle1);
			pressure1 = (float)temp * PRESSURE_SCALE;
			temp = i2cRead (pressHandle2);
			pressure2 = (float)temp * PRESSURE_SCALE;

			printf ("%f, %f\n", pressure1, pressure2);

			usleep (5000);
		}
		goto shutdown;
	}

	//Test TCP/UDP
	if (argc == 2 && argv[1][0] == 'i') {
		while (1) {
			recv (sock, buffer, 255, 0);
			printf ("%s\n", buffer);
			usleep (5000);
		}
	}

shutdown:
        pruTerminate ();

	i2c_close (pressHandle1);
	i2c_close (pressHandle2);

	close (sock);

        return 0;
}

