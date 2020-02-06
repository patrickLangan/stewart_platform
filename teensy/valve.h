#ifndef VALVE_H
#define VALVE_H

#include <stdint.h>

enum DCV_dir_ {
	DCV_CLOSE = 0,
	DCV_EXTEND,
	DCV_RETRACT
};

struct valve_state_ {
	int pos1, pos2;
	int stp1, stp2;
	int over1, over2;
	uint32_t stp_timer;
	enum DCV_dir_ DCV_pos;
	uint32_t DCV_timer;
};

void valve_init(void);

void valve_coldstart(void);

void valve_cmd(int index, struct valve_state_ *state, float travel1, float travel2);

#endif

