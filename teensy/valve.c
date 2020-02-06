#include "valve.h"

#include "util.h"

#include "WProgram.h"
#include "pins_arduino.h"

#define STP_MAX 500

/*
 * Over-tighten the valves by this amount whenever closed (accounts for skipped
 * steps, if that's even a problem with this system)
 */
#define STP_OVER 4

#define STP_PERIOD_2 950  /* 500 steps in 0.95 seconds -> 0.00095s per half-step */
#define DCV_PERIOD 333333 /* 3 cycles/sec -> 0.333s per cycle */

const int pin_SSR[4] = {35, 36, 27, 28};
const int pin_STP[4] = {23, 22, 2, 6};
const int pin_DIR[4] = {17, 14, 24, 25};

void valve_init(void)
{
	int i;

	for (i = 0; i < 4; i++) {
		pinMode(pin_SSR[i], OUTPUT);
		digitalWriteFast(pin_SSR[i], 0);
	}
	for (i = 0; i < 4; i++) {
		pinMode(pin_STP[i], OUTPUT);
		digitalWriteFast(pin_STP[i], 0);
		pinMode(pin_DIR[i], OUTPUT);
		digitalWriteFast(pin_DIR[i], 0);
	}
}

void valve_coldstart(void)
{
	int i, j;

	for (j = 0; j < 4; j++)
		digitalWriteFast(pin_DIR[j], 1);

	for (i = 0; i < STP_MAX + STP_OVER; i++) {
		for (j = 0; j < 4; j++)
			digitalWriteFast(pin_STP[j], 1);
		delayMicroseconds(STP_PERIOD_2);
		for (j = 0; j < 4; j++)
			digitalWriteFast(pin_STP[j], 0);
		delayMicroseconds(STP_PERIOD_2);
	}

	for (j = 0; j < 4; j++)
		digitalWriteFast(pin_DIR[j], 0);
}

static void DCV_switch(int index, enum DCV_dir_ dir)
{
	int SSR1, SSR2;

	SSR1 = pin_SSR[index * 2];
	SSR2 = pin_SSR[index * 2 + 1];

	switch (dir) {
	case DCV_CLOSE:
	default:
		digitalWriteFast(SSR1, 0);
		digitalWriteFast(SSR2, 0);
		break;
	case DCV_EXTEND:
		digitalWriteFast(SSR1, 0);
		digitalWriteFast(SSR2, 1);
		break;
	case DCV_RETRACT:
		digitalWriteFast(SSR1, 1);
		digitalWriteFast(SSR2, 0);
	}
}

static int valve_step(int index, int cmd, int pos, int *stp, int *over)
{
	if (pos != cmd) {
		int overtravel = 0;

		if (pos < cmd) {
			if (pos >= 2 * STP_MAX)
				return pos;
			digitalWriteFast(pin_DIR[index], 0);
			pos++;
		} else {
			if (pos <= 0)
				return pos;
			digitalWriteFast(pin_DIR[index], 1);
			if (pos == 1 && *over) {
				overtravel = 1;
				(*over)--;
			} else {
				pos--;
			}
		}

		*stp = !(*stp);
		digitalWriteFast(pin_STP[index], *stp);

		if (!overtravel)
			*over = STP_OVER;
	}

	return pos;
}

void valve_cmd(int index, struct valve_state_ *state, float travel1, float travel2)
{
	int cmd1, cmd2;
	enum DCV_dir_ DCV_cmd;
	uint32_t time;

	time = micros();

	if (limit_frequency_us(time, &state->stp_timer, STP_PERIOD_2))
		return;

	cmd1 = (int)round(travel1 * 10.0); /* 10 half-steps per percent */
	cmd2 = (int)round(travel2 * 10.0);

	/*
	 * Address shared sign (the DCV) between valve1 and valve2
	 * If sign disagrees, set the smaller value to zero - next best thing
	 */
	if (cmd1 > 0 && cmd2 < 0) {
		if (cmd1 < -cmd2)
			cmd1 = 0;
		else
			cmd2 = 0;
	} else if (cmd1 < 0 && cmd2 > 0) {
		if (-cmd1 < cmd2)
			cmd1 = 0;
		else
			cmd2 = 0;
	}

	/* If DCV needs to switch, drive valves to zero first */
	if (cmd1 > 0 || cmd2 > 0)
		DCV_cmd = DCV_EXTEND;
	else if (cmd1 < 0 || cmd2 < 0)
		DCV_cmd = DCV_RETRACT;
	else
		DCV_cmd = DCV_CLOSE;
	if (state->DCV_pos != DCV_cmd) {
		if (!state->pos1 && !state->pos2 &&
		    !limit_frequency_us(time, &state->DCV_timer, DCV_PERIOD))
		{
			DCV_switch(index, DCV_cmd);
			state->DCV_pos = DCV_cmd;
		} else {
			cmd1 = 0;
			cmd2 = 0;
		}
	}

	/*
	 * For the above calcs it makes sense for valve1,2 to share sign of DCV.
	 * The following function calls deal with setting the physical valves
	 * though, where negative travel isn't possible, hence the sign reversal
	 * on valve2.
	 */
	state->pos1 =  valve_step(index * 2 + 0,  cmd1,  state->pos1, &state->stp1, &state->over1);
	state->pos2 = -valve_step(index * 2 + 1, -cmd2, -state->pos2, &state->stp2, &state->over2);
}

