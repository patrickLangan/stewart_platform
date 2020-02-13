#ifndef COMMS_H
#define COMMS_H

#include "common.h"

#define BAUD_ 115200

#define SIZE_STATE 37
#define SIZE_CMD 14
#define SIZE_STATE_FULL 105
#define SIZE_CMD_FULL 34

#define START_BYTE 0xE

enum cmd_mode_ {
	CMD_LENGTH,
	CMD_VALVE
};

uint16_t crc16(uint8_t *buff, int n);

void pack_state(uint8_t *buff, struct board_state_ *board_state);

int unpack_state(uint8_t *buff, struct board_state_ *board_state);

void pack_state_full(uint8_t *buff, struct board_state_ *board_state);

int unpack_state_full(uint8_t *buff, struct board_state_ *board_state);

void pack_cmd(uint8_t *buff, struct board_cmd_ *board_cmd, uint8_t cmd_mode);

int unpack_cmd(uint8_t *buff, struct board_cmd_ *board_cmd, uint8_t *cmd_mode);

void pack_cmd_full(uint8_t *buff, struct board_cmd_ *board_cmd, uint8_t cmd_mode);

int unpack_cmd_full(uint8_t *buff, struct board_cmd_ *board_cmd, uint8_t *cmd_mode);

int get_packet_unbuffered(uint8_t *buff, int (*getchar)(void), int n, int buff_size);

int get_packet(uint8_t *buff, int *got_startb, int (*getchar)(void), int n, int buff_size);

#endif

