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
#define LENGTH_SCALE 3.906379093e-4

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

int inetClientInit (const char *address)
{
        int sock;
        struct sockaddr_in server;

        if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
                fprintf (stderr, "socket(AF_INET, SOCK_STREAM, 0) failed\n");
                return -1;
        }

        server.sin_addr.s_addr = inet_addr (address);
        server.sin_family = AF_INET;
        server.sin_port = htons (8888);

        if (connect (sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
                fprintf (stderr, "connect() failed\n");
                return -1;
        }

        return sock;
}

int main (int argc, char **argv)
{
        int sock;
        char buffer[255];

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

        pressHandle1 = i2c_open (1, 0x28);
        pressHandle2 = i2c_open (2, 0x28);

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
			printf ("%d\n", temp);

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

shutdown:
        pruTerminate ();

	i2c_close (pressHandle1);
	i2c_close (pressHandle2);

        return 0;
}

