#include "control.h"

#include <math.h>
#include "util.h"

#define CONTROL_PERIOD 10000 /* 100 Hz */

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

const float valve_velmax = 66.67; /* %/s */
const float valve_trvmax = 100.0; /* % */

static void gain_schedule(float K[2][6], float x10)
{
	float findex;
	float slope;
	int index1, index2;
	int i;

	findex = x10 / L * (K_ROW - 1);
	index1 = (int)findex * K_COL;
	index2 = index1 + K_COL;

	slope = findex - (float)(index1 / K_COL);

	for (i = 0; i < 12; i++) {
		int row, col;
		row = i > 5;
		col = i - 6 * row;
		K[row][col] = (Ks[index2 + i] - Ks[index1 + i]) * slope + Ks[index1 + i];
	}
}

/*
 * Find control input based on system state
 */
void control_advance(struct control_data_ *data, float L, float L_dot,
                     float p1, float p2, float setpoint, uint32_t time)
{
	float x1, x2, x3, x4, x5, x6;
	float x10, x30, x40;
	float w1, w2;
	float K[2][6];
	float delta_t;

	/* Calculate time delta since last iteration, return if too short */
	{
		uint32_t idelta_t;
		idelta_t = time_delta(time, data->timer);
		if (idelta_t < CONTROL_PERIOD)
			return;
		data->timer = time;
		delta_t = (float)idelta_t / 1e6;
	}

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

	/* Find system state based on sensor readings */
	x1 = L - x10;
	x2 = L_dot;
	x3 = p2 * (x1 + x10) * A1 / (R * T) - x30;
	x4 = p1 * (L - x1 - x10) * A2 / (R * T) - x40;
	x5 += w1 * delta_t;
	x6 += (w1 + w2) * delta_t;

	/* Physical constraints on system state */
	x1 = flimit(x1, -x10, L - x10);
	x5 = flimit(x5, -valve_trvmax, valve_trvmax);
	x6 = flimit(x6, -valve_trvmax, valve_trvmax);

	/*
	 * Schedule feedback gain matrix based on position setpoint.
	 * Lookup table with linear interpolation.
	 */
	gain_schedule(K, x10);

	/* Find control input by multiplying state by feedback gain matrix: u = -Kx */
	w1 = -K[0][0] * x1 - K[0][1] * x2 - K[0][2] * x3 - K[0][3] * x4 - K[0][4] * x5 - K[0][5] * x6;
	w2 = -K[1][0] * x1 - K[1][1] * x2 - K[1][2] * x3 - K[1][3] * x4 - K[1][4] * x5 - K[1][5] * x6;

	/* Physical constraints on control input */
	w1 = flimit(w1, -valve_velmax, valve_velmax);
	w2 = flimit(w1, -valve_velmax - w1, valve_velmax - w1);

	data->w1 = w1;
	data->w2 = w2;
	data->x5 = x5;
	data->x6 = x6;
}

