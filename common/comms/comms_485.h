#ifndef COMMS_485_H
#define COMMS_485_H

#include "comms.h"

void comms_485_init(void);

int comms_485_send(int index, struct board_cmd_ *board_cmd, uint8_t cmd_mode);

int comms_485_recv(int index, struct board_state_ *board_state);

int comms_485_slave(struct board_state_ *board_state, struct board_cmd_ *board_cmd, uint8_t *cmd_mode);

#endif

