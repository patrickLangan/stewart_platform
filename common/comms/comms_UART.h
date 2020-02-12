#ifndef COMMS_UART_H
#define COMMS_UART_H

#include "comms.h"
#include "common.h"

int comms_UART_init(void);

#if BOARD == BBB
int comms_UART_send_cmd(struct board_cmd_ *board_cmd, uint8_t cmd_mode);

int comms_UART_recv_state(struct board_state_ *board_state, int *got_startb);
#else
int comms_UART_recv_cmd(struct board_cmd_ *board_cmd, int *got_startb, uint8_t *cmd_mode);

int comms_UART_send_state(struct board_state_ *board_state);
#endif


#endif

