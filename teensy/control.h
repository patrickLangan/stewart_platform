#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>

struct control_data_ {
	float w1, w2;
	float x5, x6;
	uint32_t timer;
};

void control_advance(struct control_data_ *data, float L, float L_dot,
                     float p1, float p2, float setpoint, uint32_t time);

#endif

