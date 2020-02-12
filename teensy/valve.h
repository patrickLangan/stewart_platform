#ifndef VALVE_H
#define VALVE_H

#include <stdint.h>
#include "common.h"

struct valve_state_ {
	int pos1, pos2;
	int cmd1, cmd2;
	int stp1, stp2;
	int over1, over2;
	uint32_t stp_timer;
	enum DCV_pos_ DCV_pos;
	enum DCV_pos_ DCV_cmd;
	uint32_t DCV_timer;
};

void valve_init(void);

void valve_coldstart(void);

void DCV_switch(int index, enum DCV_pos_ dir);

int valve_step(int index, int cmd, int pos, int *stp, int *over);

void valve_control_input(int index, struct valve_state_ *state, float u1, float u2);

#endif

