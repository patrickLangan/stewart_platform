#include "valve.h"

#include "util.h"

#include "WProgram.h"
#include "pins_arduino.h"

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

void valve_init(struct valve_ *valve, struct DCV_ *DCV)
{
	int i;

	for (i = 0; i < 2; i++)
		DCV[i].index = i;
	for (i = 0; i < 4; i++) {
		valve[i].index = i;
		pinMode(pin_SSR[i], OUTPUT);
		digitalWriteFast(pin_SSR[i], 0);
		pinMode(pin_STP[i], OUTPUT);
		digitalWriteFast(pin_STP[i], 0);
		pinMode(pin_DIR[i], OUTPUT);
		digitalWriteFast(pin_DIR[i], 0);
	}
}

void DCV_switch(struct DCV_ *DCV)
{
	int SSR1, SSR2;

	if (limit_frequency_us(micros(), &DCV->timer, DCV_PERIOD))
		return;

	SSR1 = pin_SSR[DCV->index * 2];
	SSR2 = pin_SSR[DCV->index * 2 + 1];

	switch (DCV->cmd) {
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

	DCV->pos = DCV->cmd;
}

void valve_step(struct valve_ *valve)
{
	int overtravel = 0;
	int index;

	if (limit_frequency_us(micros(), &valve->timer, STP_PERIOD_2))
		return;

	index = valve->index;

	if (valve->pos != valve->cmd) {
		if (valve->pos < valve->cmd) {
			if (valve->pos >= STP_MAX)
				return;
			digitalWriteFast(pin_DIR[index], 0);
			valve->pos++;
		} else {
			digitalWriteFast(pin_DIR[index], 1);
			if (valve->cmd < 0) {
				if (!valve->over)
					return;
				overtravel = 1;
				if (valve->last_cmd >= 0)
					valve->over = STP_MAX;
				if (valve->pos > 0)
					valve->pos--;
				else
					valve->over--;
			} else if (valve->pos == 1 && valve->over) {
				overtravel = 1;
				valve->over--;
			} else {
				valve->pos--;
			}
		}

		valve->stp = !valve->stp;
		digitalWriteFast(pin_STP[index], valve->stp);
	}

	valve->last_cmd = valve->cmd;

	if (!overtravel)
		valve->over = STP_OVER;
}

void valve_control_input(struct valve_ *valve1, struct valve_ *valve2, struct DCV_ *DCV, float u1, float u2)
{
	int cmd1, cmd2;
	enum DCV_pos_ DCV_cmd;

	cmd1 = (int)round(u1 * 10.0); /* 10 half-steps per percent */
	cmd2 = (int)round(u2 * 10.0);

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
	if (DCV->pos != DCV_cmd) {
		if (!valve1->pos && !valve2->pos) {
			DCV->cmd = DCV_cmd;
			DCV_switch(DCV);
		}
		cmd1 = 0;
		cmd2 = 0;
	}

	valve1->cmd = abs(cmd1);
	valve_step(valve1);
	valve2->cmd = abs(cmd2);
	valve_step(valve2);
}

