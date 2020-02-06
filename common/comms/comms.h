#ifndef COMMS_H
#define COMMS_H

#include <stdint.h>

#define BAUD_ 115200

#define SIZE_STATE 27
#define SIZE_CMD 11
#define SIZE_STATE_FULL 75
#define SIZE_CMD_FULL 27

#define START_BYTE 0xE

struct board_state_ {
	float pressure[4];
	float length[2];
};

struct board_cmd_ {
	float length[2];
};

uint16_t crc16(uint8_t *buff, int n);

void pack_state(uint8_t *buff, struct board_state_ *board_state);

int unpack_state(uint8_t *buff, struct board_state_ *board_state);

void pack_state_full(uint8_t *buff, struct board_state_ *board_state);

int unpack_state_full(uint8_t *buff, struct board_state_ *board_state);

void pack_cmd(uint8_t *buff, struct board_cmd_ *board_cmd);

int unpack_cmd(uint8_t *buff, struct board_cmd_ *board_cmd);

void pack_cmd_full(uint8_t *buff, struct board_cmd_ *board_cmd);

int unpack_cmd_full(uint8_t *buff, struct board_cmd_ *board_cmd);

int get_packet(uint8_t *buff, int *got_startb, int (*getchar)(void), int n, int buff_size);

#endif

