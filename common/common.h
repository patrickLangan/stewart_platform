#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define TEENSY1 0
#define TEENSY2 1
#define TEENSY3 2
#define BBB 4

#if BOARD < 0 || BOARD > 4
#error
#endif

#define PA_TO_PSI 0.0001450377

#define STP_TO_PERCENT 0.1
#define STP_MAX 1000

enum DCV_pos_ {
	DCV_RETRACT = -1,
	DCV_CLOSE,
	DCV_EXTEND
};

struct board_state_ {
	float length[2];
	float length_dot[2];
	float pressure[4];
	int16_t valve[4];
	int8_t DCV[2];
};

struct board_cmd_ {
	float length[2];
	int16_t valve[4];
	int8_t DCV[2];
};

#endif

