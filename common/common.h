#ifndef COMMON_H
#define COMMON_H

#define TEENSY1 0
#define TEENSY2 1
#define TEENSY3 2
#define BBB 4

#if BOARD < 0 || BOARD > 4
#error
#endif

#define PA_TO_PSI 0.0001450377

struct board_state_ {
	float pressure[4];
	float length[2];
	int valve_pos[4];
	int valve_cmd[4];
	int DCV_pos[2];
};

struct board_cmd_ {
	float length[2];
	int valve_cmd[4];
	int DCV_cmd[2];
};

#endif

