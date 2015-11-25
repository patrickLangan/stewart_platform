#include <stdio.h>
#include <stdlib.h>

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
        int pressHandle1;
        int pressHandle2;

        int sock;
        char buffer[255];

        int temp;
        float force;
        float length1;
        float length2;
        float atmPressure;
        float pressure1;
        float pressure2;

        struct timespec waitTime = {0, 100000000};

	double lastTime;
	struct timeval curTimeval;
	double curTime = 0.0;

	float lastX = 0.0;

	float x;
	float v;
	float n1;
	float n2;

	float x_0;
	float v_0;
	float n1_0;
	float n2_0;

	float u1;
	float u2;

	float A1 = 0.0018288973;
	float A2 = 0.0020268299;
	float R = 8.314;
	float T = 296.15;
	float L = 0.762;
	float Pr = 577.4825;
	float Patm = 101325.0;
	float m = 15.8757;
	float g = 9.81;

	float K[2][4];
	float Kext[808];
	float Kret[808];

	FILE *file;

	float zeroLength1 = 0.0;
	float zeroLength2 = 0.0;

	int moving = 0;
	float deadband = 0.00254;

	float index;
	int i, j;

        if (setjmp (buf))
                goto shutdown;

        signal (SIGINT, signalCatcher);

	if (argc != 2 || (argv[1][0] != 'c' && argv[1][0] != 'r' && argv[1][0] != 'e' && argv[1][0] != 'g')) {
		fprintf (stderr, "Expects either c, r, e, or g as an argument\n");
		goto shutdown;
	}

        pruInit ();

	if (argv[1][0] == 'c') { 
		if (prussdrv_exec_program (1, "./coldStart.bin")) {
			fprintf (stderr, "prussdrv_exec_program(1, './coldStart.bin') failed\n");
			return 1;
		}
		sleep (10);
		goto shutdown;
	}

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

	file = fopen ("Kext.bin", "rb");
	fread (Kext, sizeof (*Kext), 808, file);
	fclose (file);

	file = fopen ("Kret.bin", "rb");
	fread (Kret, sizeof (*Kret), 808, file);
	fclose (file);

        pressHandle1 = i2c_open (1, 0x28);
        pressHandle2 = i2c_open (2, 0x28);

	gettimeofday (&curTimeval, NULL);
	curTime = (double)curTimeval.tv_sec + (double)curTimeval.tv_usec / 1e6;

        //sock = inetClientInit ("192.168.1.102");

	//Retract cylinder
	if (argv[1][0] == 'r') { 
		while (1) {
			lastTime = curTime;
			gettimeofday (&curTimeval, NULL);
			curTime = (double)curTimeval.tv_sec + (double)curTimeval.tv_usec / 1e6;

			pruDataMem1_int[0] = -200;
			pruDataMem1_int[1] = -200;

			temp = i2cRead (pressHandle1);
			pressure1 = (float)temp * PRESSURE_SCALE;
			temp = i2cRead (pressHandle2);
			pressure2 = (float)temp * PRESSURE_SCALE;

			temp = pruDataMem0_int[0];
			if (temp & (1 << 17))
				temp |= 0xFFFC0000;
			length1 = (float)temp * LENGTH_SCALE;

			temp = pruDataMem0_int[1];
			if (temp & (1 << 17))
				temp |= 0xFFFC0000;
			length2 = (float)temp * LENGTH_SCALE;

			lastX = x;
			x = (length1 - 0.522283) * 0.0254;
			v = (x - lastX) / (curTime - lastTime);
			n1 = pressure2 * 6894.76 * x * A1 / (R * T);
			n2 = pressure1 * 6894.76 * (L - x) * A2 / (R * T);

			printf ("%f, %f, %f, %f\n", length1, length2, pressure1, pressure2);

			usleep (5000);
		}
	}

	//Extend cylinder
	if (argv[1][0] == 'e') { 
		while (1) {
			lastTime = curTime;
			gettimeofday (&curTimeval, NULL);
			curTime = (double)curTimeval.tv_sec + (double)curTimeval.tv_usec / 1e6;

			pruDataMem1_int[0] = 300;
			pruDataMem1_int[1] = 300;

			temp = i2cRead (pressHandle1);
			pressure1 = (float)temp * PRESSURE_SCALE;
			temp = i2cRead (pressHandle2);
			pressure2 = (float)temp * PRESSURE_SCALE;

			temp = pruDataMem0_int[0];
			if (temp & (1 << 17))
				temp |= 0xFFFC0000;
			length1 = (float)temp * LENGTH_SCALE;

			temp = pruDataMem0_int[1];
			if (temp & (1 << 17))
				temp |= 0xFFFC0000;
			length2 = (float)temp * LENGTH_SCALE;

			lastX = x;
			x = (length1 - 0.522283) * 0.0254;
			v = (x - lastX) / (curTime - lastTime);
			n1 = pressure2 * 6894.76 * x * A1 / (R * T);
			n2 = pressure1 * 6894.76 * (L - x) * A2 / (R * T);

			printf ("%f, %f, %f, %f\n", length1, length2, pressure1, pressure2);

			usleep (5000);
		}
	}

	//Gain scheduling controller
	if (argv[1][0] == 'g') { 
		while (1) {
			usleep (5000);

			lastTime = curTime;
			gettimeofday (&curTimeval, NULL);
			curTime = (double)curTimeval.tv_sec + (double)curTimeval.tv_usec / 1e6;

			temp = i2cRead (pressHandle1);
			pressure1 = (float)temp * PRESSURE_SCALE;
			temp = i2cRead (pressHandle2);
			pressure2 = (float)temp * PRESSURE_SCALE;

			temp = pruDataMem0_int[0];
			if (temp & (1 << 17))
				temp |= 0xFFFC0000;
			length1 = (float)temp * LENGTH_SCALE;

			temp = pruDataMem0_int[1];
			if (temp & (1 << 17))
				temp |= 0xFFFC0000;
			length2 = (float)temp * LENGTH_SCALE;

			//recv (sock, buffer, 255, 0);
			//force = atof (buffer);

			//recv (sock, buffer, 255, 0);
			//atmPressure = atof (buffer);

			lastX = x;
			x = (length1 - zeroLength1) * 0.0254;
			v = (x - lastX) / (curTime - lastTime);
			n1 = pressure2 * 6894.76 * x * A1 / (R * T);
			n2 = pressure1 * 6894.76 * (L - x) * A2 / (R * T);

			x_0 = (length2 - zeroLength2) * 0.0254;
			if (x_0 > L)
				x_0 = L;
			if (x_0 < 0)
				x_0 = 0.0;
			n1_0 = 1000.0 * Pr * A1 * x_0 / (R * T) - 0.00001;
			n2_0 = (L - x_0) / (R * T) * (1000.0 * Pr * A1 - Patm * (A1 - A2) - m * g);

			switch (moving) {
			case 1 :
				if (x > x_0)
					moving = 0;
				break;
			case 2 :
				if (x_0 > x)
					moving = 0;
				break;
			case 0 :
				if (x_0 - x > deadband)
					moving = 1;
				else if (x - x_0 > deadband)
					moving = 2;
			}

			index = x_0 / L * 99.0;
			i = (int)index * 8;
			j = i + 8;
			if (x > x_0) {
				K[0][0] = (Kret[j + 0] - Kret[i + 0]) * (index - (float)(i / 8)) + Kret[i + 0];
				K[0][1] = (Kret[j + 1] - Kret[i + 1]) * (index - (float)(i / 8)) + Kret[i + 1];
				K[0][2] = (Kret[j + 2] - Kret[i + 2]) * (index - (float)(i / 8)) + Kret[i + 2];
				K[0][3] = (Kret[j + 3] - Kret[i + 3]) * (index - (float)(i / 8)) + Kret[i + 3];
				K[1][0] = (Kret[j + 4] - Kret[i + 4]) * (index - (float)(i / 8)) + Kret[i + 4];
				K[1][1] = (Kret[j + 5] - Kret[i + 5]) * (index - (float)(i / 8)) + Kret[i + 5];
				K[1][2] = (Kret[j + 6] - Kret[i + 6]) * (index - (float)(i / 8)) + Kret[i + 6];
				K[1][3] = (Kret[j + 7] - Kret[i + 7]) * (index - (float)(i / 8)) + Kret[i + 7];
			} else {
				K[0][0] = (Kext[j + 0] - Kext[i + 0]) * (index - (float)(i / 8)) + Kext[i + 0];
				K[0][1] = (Kext[j + 1] - Kext[i + 1]) * (index - (float)(i / 8)) + Kext[i + 1];
				K[0][2] = (Kext[j + 2] - Kext[i + 2]) * (index - (float)(i / 8)) + Kext[i + 2];
				K[0][3] = (Kext[j + 3] - Kext[i + 3]) * (index - (float)(i / 8)) + Kext[i + 3];
				K[1][0] = (Kext[j + 4] - Kext[i + 4]) * (index - (float)(i / 8)) + Kext[i + 4];
				K[1][1] = (Kext[j + 5] - Kext[i + 5]) * (index - (float)(i / 8)) + Kext[i + 5];
				K[1][2] = (Kext[j + 6] - Kext[i + 6]) * (index - (float)(i / 8)) + Kext[i + 6];
				K[1][3] = (Kext[j + 7] - Kext[i + 7]) * (index - (float)(i / 8)) + Kext[i + 7];
			}

			//u1 = (x - x_0) * K[0][0] + (v - v_0) * K[0][1] + (n1 - n1_0) * K[0][2] + (n2 - n2_0) * K[0][3];
			//u2 = (x - x_0) * K[1][0] + (v - v_0) * K[1][1] + (n1 - n1_0) * K[1][2] + (n2 - n2_0) * K[1][3];
			u1 = (n1 - n1_0) * -K[0][2] + (n2 - n2_0) * -K[0][3];
			u2 = (n1 - n1_0) * -K[1][2] + (n2 - n2_0) * -K[1][3];

			if (!moving) {
				u1 = 0.0;
				u2 = 0.0;
			}

			printf ("%d, ", moving);
			printf ("%f, %f, %f, %f, %f, %f, %f, %f\n", curTime, x_0, x, v, n1, n2, u1, u2);

			if (u1 < 0.0) {
				u1 = 0.0;
			}
			if (u1 > 100.0) {
				u1 = 100.0;
			}

			if (u2 < 0.0) {
				u2 = 0.0;
			}
			if (u2 > 100.0) {
				u2 = 100.0;
			}

			if (x > x_0) {
				u1 = u1 * -1.0;
				u2 = u2 * -1.0;
			}

			pruDataMem1_int[1] = (int)(u1 * 5.0);
			pruDataMem1_int[0] = (int)(u2 * 5.0);
		}
	}

shutdown:
	pruDataMem1_int[0] = 0;
	pruDataMem1_int[1] = 0;

	sleep (10);

        pruTerminate ();

        return 0;
}

