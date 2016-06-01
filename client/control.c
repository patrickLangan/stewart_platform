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
        FILE *file;
        char fileName[255];

	float K[2][6];
        float Ks[1212];
	float findex;
	float kslope;

	int lengthHandle;
        int pressHandle1;
        int pressHandle2;

	double lastTime;
	struct timeval curTimeval;
	double curTime = 0.0;
	double startTime;

	float g = 9.81; //m/s^2
	float R = 8.314; //j/(K*mol)
	float M = 28.97; //kg/kmol

	float N9 = 22.5;
	float Pref = 101325.0; //Pa
	float Tref = 288.15; //K

	float T = 296.15; //K
	float P0 = 101325.0; //Pa
	float Ps = 721853.4; //Pa

	float A1 = 0.0020268299; //m^2
	float A2 = 0.0017418070; //m^2
	float L = 0.762; //m
	float m = 24.5433; //kg

	float c1 = -5.0953e-06;
	float c2 = 0.0028189;
	float Xt = 0.6281276364;

	float k = 1e9;

	float valve_velmax = 111.1111; //%/s
	float valve_trvmax = 100.0; //%

	float setpoint = 0.381; //m

	float x1 = 0.0;
	float x2 = 0.0;
	float x3 = 0.0;
	float x4 = 0.0;
	float x5 = 0.0;
	float x6 = 0.0;

	float u1 = 0.0;
	float u2 = 0.0;

	float x0; //m
	float v0; //m/s
	float P10; //kPa
	float n10; //mol
	float n20; //mol

	float inchworm = 0.1016; //m

	float stepTime = 5.0;
	int toggle = 0;

	float mg;

        int temp;
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

	file = fopen ("K.bin", "rb");
        fread (Ks, sizeof (*Ks), 1212, file);
        fclose (file);

        pressHandle1 = i2c_open (1, 0x28);
        pressHandle2 = i2c_open (2, 0x28);

	mg = -469.0059787 * setpoint + 226.588527;

	x0 = setpoint;
	v0 = 0.0;
	P10 = Ps * 0.8;
	n10 = P10 * A1 * x0 / (R * T);
	n20 = (P10 * A1 - P0 * (A1 - A2) - mg) * (L - x0) / (R * T);

	temp = pruDataMem0_int[0];
	x1 = (float)(temp - LENGTH_ZERO) * LENGTH_SCALE - x0;

	gettimeofday (&curTimeval, NULL);
	curTime = (double)curTimeval.tv_sec + (double)curTimeval.tv_usec / 1e6;
	startTime = curTime;

	sprintf (fileName, "%d.csv", (int)startTime);
        file = fopen (fileName, "w");
	fprintf (file, "time, x0, x, v, n1, n2, x5, x6, u1, u2\n");

	//Gain scheduling controller
	while (1) {
		float length;
		float pressure1;
		float pressure2;

		float delta_t;
		float lastX1;

		float last_x0;
		float last_n10;
		float last_n20;

		float inchpoint;

		int out1;
		int out2;

                usleep (5000);

                lastTime = curTime;
                gettimeofday (&curTimeval, NULL);
                curTime = (double)curTimeval.tv_sec + (double)curTimeval.tv_usec / 1e6;
		delta_t = curTime - lastTime;

                temp = i2cRead (pressHandle1);
                pressure1 = (float)temp * PRESSURE_SCALE;
                temp = i2cRead (pressHandle2);
                pressure2 = (float)temp * PRESSURE_SCALE;

		temp = pruDataMem0_int[0];
		length = (float)(temp - LENGTH_ZERO) * LENGTH_SCALE;

                lastX1 = x1;
                x1 = length - x0;
                x2 = (x1 - lastX1) / delta_t;
                x3 = pressure1 * 6894.76 * (x1 + x0) * A1 / (R * T) - n10;
                x4 = pressure2 * 6894.76 * (L - x1 - x0) * A2 / (R * T) - n20;
		x5 += u1 * delta_t;
		x6 += u2 * delta_t;

		if (x1 + x0 > L)
			x1 = L - x0;
		else if (x1 + x0 < 0)
			x1 = -x0;

		if (x5 > valve_trvmax)
			x5 = valve_trvmax;
		else if (x5 < -valve_trvmax)
			x5 = -valve_trvmax;

		if (x5 + x6 > valve_trvmax)
			x6 = valve_trvmax - x5;
		else if (x5 + x6 < -valve_trvmax)
			x6 = -valve_trvmax - x5;

		if (curTime - startTime > stepTime) {
			if (toggle == 1)
				setpoint = L / 2.0;
			else
				setpoint = 0.1;
			toggle = !toggle;
			stepTime += 5.0;
		}

		inchpoint = setpoint;
		if (setpoint - x1 - x0 > inchworm)
			inchpoint = inchworm + x1 + x0;
		else if (x1 + x0 - setpoint > inchworm)
			inchpoint = x1 + x0 - inchworm;

		last_x0 = x0;
		last_n10 = n10;
		last_n20 = n20;

		mg = -469.0059787 * inchpoint + 226.588527;

		x0 = inchpoint;
		n10 = P10 * A1 * x0 / (R * T);
		n20 = (P10 * A1 - P0 * (A1 - A2) - mg) * (L - x0) / (R * T);

		x1 = x1 + last_x0 - x0;
		x3 = x3 + last_n10 - n10;
		x4 = x4 + last_n20 - n20;
		lastX1 = lastX1 + last_x0 - x0;

                findex = x0 / L * 99.0;
                i = (int)findex * 12;
                j = i + 12;
		kslope = findex - (float)(i / 12);
		K[0][0] = (Ks[j + 0] - Ks[i + 0]) * kslope + Ks[i + 0];
		K[0][1] = (Ks[j + 1] - Ks[i + 1]) * kslope + Ks[i + 1];
		K[0][2] = (Ks[j + 2] - Ks[i + 2]) * kslope + Ks[i + 2];
		K[0][3] = (Ks[j + 3] - Ks[i + 3]) * kslope + Ks[i + 3];
		K[0][4] = (Ks[j + 4] - Ks[i + 4]) * kslope + Ks[i + 4];
		K[0][5] = (Ks[j + 5] - Ks[i + 5]) * kslope + Ks[i + 5];
		K[1][0] = (Ks[j + 6] - Ks[i + 6]) * kslope + Ks[i + 6];
		K[1][1] = (Ks[j + 7] - Ks[i + 7]) * kslope + Ks[i + 7];
		K[1][2] = (Ks[j + 8] - Ks[i + 8]) * kslope + Ks[i + 8];
		K[1][3] = (Ks[j + 9] - Ks[i + 9]) * kslope + Ks[i + 9];
		K[1][4] = (Ks[j + 10] - Ks[i + 10]) * kslope + Ks[i + 10];
		K[1][5] = (Ks[j + 11] - Ks[i + 11]) * kslope + Ks[i + 11];

		u1 = -K[0][0] * x1 - K[0][1] * x2 - K[0][2] * x3 - K[0][3] * x4 - K[0][4] * x5 - K[0][5] * x6;
		u2 = -K[1][0] * x1 - K[1][1] * x2 - K[1][2] * x3 - K[1][3] * x4 - K[1][4] * x5 - K[1][5] * x6;

		if (u1 > valve_velmax)
			u1 = valve_velmax;
		else if (u1 < -valve_velmax)
			u1 = -valve_velmax;

		if (u1 + u2 > valve_velmax)
			u2 = valve_velmax - u1;
		else if (u1 + u2 < -valve_velmax)
			u2 = -valve_velmax - u1;

		out2 = (int)((x5 + x6) * 5.0);
		out1 = (int)(fabs(x5) * 5.0) * ((out2 < 0) ? -1 : 1);


		pruDataMem1_int[1] = out1;
		pruDataMem1_int[0] = out2;

		printf ("%f\n", (x1 + x0 - setpoint) / 0.0254);
                fprintf (file, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f\n", curTime - startTime, x0, x1 + x0, x2, x3 + n10, x4 + n20, x5, x6, u1, u2);
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

