#ifndef VALVE_H
#define VALVE_H

#include "common.h"

struct valve_ {
	int pos;
	int cmd;
	int stp;
	int over;
	uint32_t timer;
	int index;
};

struct DCV_ {
	enum DCV_pos_ pos;
	enum DCV_pos_ cmd;
	uint32_t timer;
	int index;
};

void valve_init(struct valve_ *valve, struct DCV_ *DCV);

void valve_coldstart(void);

void DCV_switch(struct DCV_ *DCV);

void valve_step(struct valve_ *valve);

void valve_control_input(struct valve_ *valve1, struct valve_ *valve2, struct DCV_ *DCV, float u1, float u2);

#endif

