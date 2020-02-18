#include "valve.h"

#include "util.h"

#include "WProgram.h"
#include "pins_arduino.h"

#define STP_OVER 4

#define STP_PERIOD_2 1210  /* 1.21s full travel, experimentally determined */
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

/*
 * Set position of Directional Control Valve (DCV).  Limit rate of position
 * change by DCV_PERIOD (DCV can fail if cycled faster than rating).
 */
void DCV_switch(struct DCV_ *DCV)
{
	int SSR1, SSR2;

	if (DCV->pos == DCV->cmd)
		return;

	if (limit_frequency(micros(), &DCV->timer, DCV_PERIOD))
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

/*
 * Drive stepper-valve to its commanded position one step at a time.
 * If the commanded position is zero, over-tighten the valve by STP_OVER
 * half-steps (in an attempt to account for skipped steps).
 * If the commanded position is negative, over-tighten the valve by no more than
 * STP_MAX half-steps.  This allows for manually over-tightening the valve
 * through the HMI.
 */
void valve_step(struct valve_ *valve)
{
	int overtravel = 0;

	if (valve->pos != valve->cmd) {
		int index;

		if (limit_frequency(micros(), &valve->timer, STP_PERIOD_2))
			return;

		index = valve->index;

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

/*
 * Set valves based on control input in the form:
 * -100% <= u1 <= 100%, -100% <= u2 <= 100%
 * The mapping from this form to the physically existing control input (two
 * stepper-valves and one DCV) is not exact.  The advantage of this form is
 * that it allows for control algorithms which are smooth, non-integer, and
 * unconstrained (e.g. LQR).  The disadvantage is un-modeled behavior when
 * sign(u1) != sign(u2).
 */
void valve_control_input(struct valve_ *valve1, struct valve_ *valve2, struct DCV_ *DCV, float u1, float u2)
{
	int cmd1, cmd2;
	enum DCV_pos_ DCV_cmd;

	/* Convert from percent travel command to half-step count. */
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

