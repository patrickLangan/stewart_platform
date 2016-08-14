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
	float server_time = 0.0;

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
	float m = 15.6881; //kg

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

	float w1 = 0.0;
	float w2 = 0.0;

	float x10; //m
	float P10; //Pa
	float x30; //mol
	float x40; //mol

	float inchworm = 0.1016; //m

	float mg = 125.0; //N

	int ctrlvalve_last_sign = 0;
	double ctrlvalve_last_flop = 0.0;
	float ctrlvalve_min_period = 0.333333;

	char buffer[12];
	int sock;

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

	sock = udpInit(1681);

	x10 = setpoint;
	P10 = Ps * 0.8;
	x30 = P10 * A1 * x10 / (R * T);
	x40 = (P10 * A1 - P0 * (A1 - A2) - mg) * (L - x10) / (R * T);

	temp = pruDataMem0_int[0];
	x1 = (float)(temp - LENGTH_ZERO) * LENGTH_SCALE - x10;

	gettimeofday (&curTimeval, NULL);
	curTime = (double)curTimeval.tv_sec + (double)curTimeval.tv_usec / 1e6;
	startTime = curTime;

	sprintf (fileName, "%d.csv", (int)startTime);
        file = fopen (fileName, "w");
	fprintf (file, "server_time, client_time, x10, x, v, n1, n2, x5, x6, w1, w2\n");

	//Gain scheduling controller
	while (1) {
		float length;
		float pressure1;
		float pressure2;

		float delta_t;
		float lastX1;

		float last_x10;
		float last_x30;
		float last_x40;

		float inchpoint;

		int ctrlvalve_sign;

		int out1;
		int out2;

                usleep (5000);

                lastTime = curTime;
                gettimeofday (&curTimeval, NULL);
                curTime = (double)curTimeval.tv_sec + (double)curTimeval.tv_usec / 1e6;
		delta_t = curTime - lastTime;

		if (recv(sock, buffer, 12, 0) == 12) {
			setpoint = *((float *)buffer);
			mg = *((float *)buffer + 1);
			server_time = *((float *)buffer + 2);
		}

                temp = i2cRead (pressHandle1);
                pressure1 = (float)temp * PRESSURE_SCALE;
                temp = i2cRead (pressHandle2);
                pressure2 = (float)temp * PRESSURE_SCALE;

		temp = pruDataMem0_int[0];
		length = (float)(temp - LENGTH_ZERO) * LENGTH_SCALE;

                lastX1 = x1;
                x1 = length - x10;
                x2 = (x1 - lastX1) / delta_t;
                x3 = pressure2 * 6894.76 * (x1 + x10) * A1 / (R * T) - x30;
                x4 = pressure1 * 6894.76 * (L - x1 - x10) * A2 / (R * T) - x40;
		x5 += w1 * delta_t;
		x6 += (w1 + w2) * delta_t;

		if (x1 + x10 > L)
			x1 = L - x10;
		else if (x1 + x10 < 0)
			x1 = -x10;

		if (x5 > valve_trvmax)
			x5 = valve_trvmax;
		else if (x5 < -valve_trvmax)
			x5 = -valve_trvmax;

		if (x6 > valve_trvmax)
			x6 = valve_trvmax;
		else if (x6 < -valve_trvmax)
			x6 = -valve_trvmax;

		inchpoint = setpoint;
/*
		if (setpoint - x1 - x10 > inchworm)
			inchpoint = inchworm + x1 + x10;
		else if (x1 + x10 - setpoint > inchworm)
			inchpoint = x1 + x10 - inchworm;
*/

		last_x10 = x10;
		last_x30 = x30;
		last_x40 = x40;

		x10 = inchpoint;
		x30 = P10 * A1 * x10 / (R * T);
		x40 = (P10 * A1 - P0 * (A1 - A2) - mg) * (L - x10) / (R * T);

		x1 = x1 + last_x10 - x10;
		x3 = x3 + last_x30 - x30;
		x4 = x4 + last_x40 - x40;
		lastX1 = lastX1 + last_x10 - x10;

                findex = x10 / L * 99.0;
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

		w1 = -K[0][0] * x1 - K[0][1] * x2 - K[0][2] * x3 - K[0][3] * x4 - K[0][4] * x5 - K[0][5] * x6;
		w2 = -K[1][0] * x1 - K[1][1] * x2 - K[1][2] * x3 - K[1][3] * x4 - K[1][4] * x5 - K[1][5] * x6;

		if (w1 > valve_velmax)
			w1 = valve_velmax;
		else if (w1 < -valve_velmax)
			w1 = -valve_velmax;

		if (w1 + w2 > valve_velmax)
			w2 = valve_velmax - w1;
		else if (w1 + w2 < -valve_velmax)
			w2 = -valve_velmax - w1;

		out2 = (int)(x6 * 5.0);
		out1 = (int)(fabs(x5) * 5.0) * ((out2 < 0) ? -1 : 1);

		if (out2 < 0) {
			ctrlvalve_sign = -1;
		} else if (out2 > 0) {
			ctrlvalve_sign = 1;
		} else {
			ctrlvalve_sign = 0;
		}

		if (ctrlvalve_sign != ctrlvalve_last_sign && (curTime - ctrlvalve_last_flop) > ctrlvalve_min_period)
			goto ctrl1;
		else if (ctrlvalve_sign == ctrlvalve_last_sign)
			goto ctrl2;
		goto ctrl3;
	ctrl1:
		ctrlvalve_last_flop = curTime;
	ctrl2:
		pruDataMem1_int[1] = out1;
		pruDataMem1_int[0] = out2;
		ctrlvalve_last_sign = ctrlvalve_sign;
	ctrl3:
		;

                fprintf (file, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f\n", server_time, curTime - startTime, x10, x1 + x10, x2, x3 + x30, x4 + x40, x5, x6, w1, w2);
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

