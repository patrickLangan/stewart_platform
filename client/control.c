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
#define LENGTH_ZERO 12.0

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
        FILE *file;
        char fileName[255];

	float K[2][4];
        float Kext[808];
        float Kret[808];

	int lengthHandle;
        int pressHandle1;
        int pressHandle2;

	double lastTime;
	struct timeval curTimeval;
	double curTime = 0.0;
	double startTime;

        float length;
        float pressure1;
        float pressure2;

	float x = 0.0;
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

	float lastX;
	float setpoint;
        float inchwormLength = 0.0508;

        int temp;
        float index;
        int i, j;

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
	startTime = curTime;

	sprintf (fileName, "%d.csv", (int)startTime);
        file = fopen (fileName, "w");

	//Choose setpoint
	if (argc == 2)
		switch (argv[1][0]) {
		case 'z':
			setpoint = 0.1905;
			break;
		case 'x':
			setpoint = 0.5715;
			break;
		default :
			setpoint = 0.381;
		}
	else
		setpoint = 0.381;

	//Gain scheduling controller
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
		length = (float)temp * LENGTH_SCALE - LENGTH_ZERO;

                lastX = x;
                x = length * 0.0254;
                v = (x - lastX) / (curTime - lastTime);
                n1 = pressure2 * 6894.76 * x * A1 / (R * T);
                n2 = pressure1 * 6894.76 * (L - x) * A2 / (R * T);

		x_0 = setpoint;

                if (x > x_0 && x - x_0 > inchwormLength)
                        x_0 = x - inchwormLength;
                else if (x_0 > x && x_0 - x > inchwormLength)
                        x_0 = x + inchwormLength;

                n1_0 = 1000.0 * Pr * A1 * x_0 / (R * T) - 0.00001;
                n2_0 = (L - x_0) / (R * T) * (1000.0 * Pr * A1 - Patm * (A1 - A2) - m * g);

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

                u1 = (x - x_0) * -K[0][0] + (v - v_0) * -K[0][1] + (n1 - n1_0) * -K[0][2] + (n2 - n2_0) * -K[0][3];
                u2 = (x - x_0) * -K[1][0] + (v - v_0) * -K[1][1] + (n1 - n1_0) * -K[1][2] + (n2 - n2_0) * -K[1][3];

                if (u1 < 0.0)
                        u1 = 0.0;
                if (u1 > 100.0)
                        u1 = 100.0;

                if (u2 < 0.0)
                        u2 = 0.0;
                if (u2 > 100.0)
                        u2 = 100.0;

                if (x > x_0) {
                        u1 *= -1.0;
                        u2 *= -1.0;
                }

                pruDataMem1_int[1] = (int)(u1 * 5.0);
                pruDataMem1_int[0] = (int)(u2 * 5.0);

                fprintf (file, "%f, %f, %f, %f, %f, %f, %f, %f\n", curTime - startTime, x_0, x, v, n1, n2, u1, u2);
        }

shutdown:
	pruDataMem1_int[0] = 0;
        pruDataMem1_int[1] = 0;

        sleep (10);

        pruTerminate ();

	fclose (file);

	i2c_close (pressHandle1);
	i2c_close (pressHandle2);

        return 0;
}

