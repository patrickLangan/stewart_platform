#include "control.h"

#include <math.h>
#include "util.h"

#define CONTROL_PERIOD 10000 /* 100 Hz */
#define DELTA_T 0.01

#define K_COL 12
#define K_ROW 100

#define P10 (Ps * 0.8)

const float Ks[1200] = {
#include "K.csv"
};

const float R = 8.314; /* j/(K*mol) */
const float T = 296.15; /* K */
const float P0 = 101325.0; /* Pa */
const float Ps = 721853.4; /* Pa */
const float A1 = 0.0020268299; /* m^2 */
const float A2 = 0.0017418070; /* m^2 */
const float L = 0.762; /* m */
const float mg = 125.0; /* N */

const float valve_velmax = 82.64; /* %/s */
const float valve_trvmax = 100.0; /* % */

static void gain_schedule(float K[2][6], float x10)
{
	float findex;
	float slope;
        int i, j;

	findex = x10 / L * (K_ROW - 1);
	i = (int)findex * K_COL;
	j = i + K_COL;

	slope = findex - (float)(i / K_COL);

	K[0][0] = (Ks[j +  0] - Ks[i +  0]) * slope + Ks[i +  0];
	K[0][1] = (Ks[j +  1] - Ks[i +  1]) * slope + Ks[i +  1];
	K[0][2] = (Ks[j +  2] - Ks[i +  2]) * slope + Ks[i +  2];
	K[0][3] = (Ks[j +  3] - Ks[i +  3]) * slope + Ks[i +  3];
	K[0][4] = (Ks[j +  4] - Ks[i +  4]) * slope + Ks[i +  4];
	K[0][5] = (Ks[j +  5] - Ks[i +  5]) * slope + Ks[i +  5];
	K[1][0] = (Ks[j +  6] - Ks[i +  6]) * slope + Ks[i +  6];
	K[1][1] = (Ks[j +  7] - Ks[i +  7]) * slope + Ks[i +  7];
	K[1][2] = (Ks[j +  8] - Ks[i +  8]) * slope + Ks[i +  8];
	K[1][3] = (Ks[j +  9] - Ks[i +  9]) * slope + Ks[i +  9];
	K[1][4] = (Ks[j + 10] - Ks[i + 10]) * slope + Ks[i + 10];
	K[1][5] = (Ks[j + 11] - Ks[i + 11]) * slope + Ks[i + 11];
}

void control_advance(struct control_data_ *data, float L, float L_dot,
                     float p1, float p2, float setpoint, uint32_t time)
{
	float x1, x2, x3, x4, x5, x6;
	float x10, x30, x40;
	float w1, w2;
	float K[2][6];

	if (limit_frequency(time, &data->timer, CONTROL_PERIOD))
		return;

	w1 = data->w1;
	w2 = data->w2;
	x5 = data->x5;
	x6 = data->x6;

	/*
	 * Find operating point (the setpoint and new zero point for control
	 * system coordinates)
	 */
	x10 = setpoint;
	x30 = P10 * A1 * x10 / (R * T);
	x40 = (P10 * A1 - P0 * (A1 - A2) - mg) * (L - x10) / (R * T);

	/* Find control system state */
	x1 = L - x10;
	x2 = L_dot;
	x3 = p2 * (x1 + x10) * A1 / (R * T) - x30;
	x4 = p1 * (L - x1 - x10) * A2 / (R * T) - x40;
	x5 += w1 * DELTA_T;
	x6 += (w1 + w2) * DELTA_T;

	/* Physical constraints on system state */
	x1 = flimit(x1, -x10, L - x10);
	x5 = flimit(x5, -valve_trvmax, valve_trvmax);
	x6 = flimit(x6, -valve_trvmax, valve_trvmax);

	gain_schedule(K, x10);

	/* Find control input by multiplying state by feedback gain matrix: u = -Kx */
	w1 = -K[0][0] * x1 - K[0][1] * x2 - K[0][2] * x3 - K[0][3] * x4 - K[0][4] * x5 - K[0][5] * x6;
	w2 = -K[1][0] * x1 - K[1][1] * x2 - K[1][2] * x3 - K[1][3] * x4 - K[1][4] * x5 - K[1][5] * x6;

	/* Physical constraints control input */
	w1 = flimit(w1, -valve_velmax, valve_velmax);
	w2 = flimit(w1, -valve_velmax - w1, valve_velmax - w1);

	data->w1 = w1;
	data->w2 = w2;
	data->x5 = x5;
	data->x6 = x6;
}

