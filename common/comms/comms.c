#include "comms.h"

#include <string.h>

//http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html#ch5
uint16_t crc16(uint8_t *buff, int n)
{
	uint16_t crc = 0;
	int i, j;

	for (i = 0; i < n; i++) {
		crc ^= (uint16_t)(buff[i] << 8);

		for (j = 0; j < 8; j++) {
			if ((crc & 0x8000) != 0)
				crc = (uint16_t)((crc << 1) ^ 0x1021);
			else
				crc <<= 1;
		}
	}

	return crc;
} 

void pack_state(uint8_t *buff, struct board_state_ *board_state)
{
	uint16_t crc;
	buff[0] = START_BYTE;
	memcpy(buff + 1, board_state->length, 8);
	memcpy(buff + 9, board_state->pressure, 16);
	memcpy(buff + 25, board_state->valve, 8);
	memcpy(buff + 33, board_state->DCV, 2);
	crc = crc16(buff, 35);
	buff[35] = crc >> 8;
	buff[36] = crc & 0xFF;
}

int unpack_state(uint8_t *buff, struct board_state_ *board_state)
{
	uint16_t crc_recv, crc_calc;

	crc_recv = buff[35] << 8 | buff[36];
	crc_calc = crc16(buff, 35);
	if (crc_recv != crc_calc)
		return 1;

	memcpy(board_state->length, buff + 1, 8);
	memcpy(board_state->pressure, buff + 9, 16);
	memcpy(board_state->valve, buff + 25, 8);
	memcpy(board_state->DCV, buff + 33, 2);

	return 0;
}

void pack_state_full(uint8_t *buff, struct board_state_ *board_state)
{
	uint16_t crc;
	buff[0] = START_BYTE;
	memcpy(buff + 1, board_state[0].length, 8);
	memcpy(buff + 9, board_state[0].pressure, 16);
	memcpy(buff + 25, board_state[0].valve, 8);
	memcpy(buff + 33, board_state[0].DCV, 2);
	memcpy(buff + 35, board_state[1].length, 8);
	memcpy(buff + 43, board_state[1].pressure, 16);
	memcpy(buff + 59, board_state[1].valve, 8);
	memcpy(buff + 67, board_state[1].DCV, 2);
	memcpy(buff + 69, board_state[2].length, 8);
	memcpy(buff + 77, board_state[2].pressure, 16);
	memcpy(buff + 93, board_state[2].valve, 8);
	memcpy(buff + 101, board_state[2].DCV, 2);
	crc = crc16(buff, 103);
	buff[103] = crc >> 8;
	buff[104] = crc & 0xFF;
}

int unpack_state_full(uint8_t *buff, struct board_state_ *board_state)
{
	uint16_t crc_recv, crc_calc;

	crc_recv = buff[103] << 8 | buff[104];
	crc_calc = crc16(buff, 103);
	if (crc_recv != crc_calc)
		return 1;

	memcpy(board_state[0].length, buff + 1, 8);
	memcpy(board_state[0].pressure, buff + 9, 16);
	memcpy(board_state[0].valve, buff + 25, 8);
	memcpy(board_state[0].DCV, buff + 33, 2);
	memcpy(board_state[1].length, buff + 35, 8);
	memcpy(board_state[1].pressure, buff + 43, 16);
	memcpy(board_state[1].valve, buff + 59, 8);
	memcpy(board_state[1].DCV, buff + 67, 2);
	memcpy(board_state[2].length, buff + 69, 8);
	memcpy(board_state[2].pressure, buff + 77, 16);
	memcpy(board_state[2].valve, buff + 93, 8);
	memcpy(board_state[2].DCV, buff + 101, 2);

	return 0;
}

void pack_cmd(uint8_t *buff, struct board_cmd_ *board_cmd, uint8_t cmd_mode)
{
	uint16_t crc;
	buff[0] = START_BYTE;
	switch (cmd_mode) {
	case CMD_LENGTH:
		memset(buff + 1, 0, 1);
		memcpy(buff + 2, board_cmd->length, 8);
		memset(buff + 10, 0, 2);
		break;
	case CMD_VALVE:
		memset(buff + 1, 1, 1);
		memcpy(buff + 2, board_cmd->valve, 8);
		memcpy(buff + 10, board_cmd->DCV, 2);
	}
	crc = crc16(buff, 12);
	buff[12]  = crc >> 8;
	buff[13] = crc & 0xFF;
}

int unpack_cmd(uint8_t *buff, struct board_cmd_ *board_cmd, uint8_t *cmd_mode)
{
	uint16_t crc_recv, crc_calc;

	crc_recv = buff[12] << 8 | buff[13];
	crc_calc = crc16(buff, 12);
	if (crc_recv != crc_calc)
		return 1;

	memcpy(cmd_mode, buff + 1, 1);
	switch (*cmd_mode) {
	case CMD_LENGTH:
		memcpy(board_cmd->length, buff + 2, 8);
		break;
	case CMD_VALVE:
		memcpy(board_cmd->valve, buff + 2, 8);
		memcpy(board_cmd->DCV, buff + 10, 2);
	}

	return 0;
}

void pack_cmd_full(uint8_t *buff, struct board_cmd_ *board_cmd, uint8_t cmd_mode)
{
	uint16_t crc;
	buff[0] = START_BYTE;
	switch (cmd_mode) {
	case CMD_LENGTH:
		memset(buff + 1, 0, 1);
		memcpy(buff + 2, board_cmd[0].length, 8);
		memcpy(buff + 10, board_cmd[1].length, 8);
		memcpy(buff + 18, board_cmd[2].length, 8);
		memset(buff + 26, 0, 6);
		break;
	case CMD_VALVE:
		memset(buff + 1, 1, 1);
		memcpy(buff + 2, board_cmd[0].valve, 8);
		memcpy(buff + 10, board_cmd[0].DCV, 2);
		memcpy(buff + 12, board_cmd[1].valve, 8);
		memcpy(buff + 20, board_cmd[1].DCV, 2);
		memcpy(buff + 22, board_cmd[2].valve, 8);
		memcpy(buff + 30, board_cmd[2].DCV, 2);
	}
	crc = crc16(buff, 32);
	buff[32]  = crc >> 8;
	buff[33] = crc & 0xFF;
}

int unpack_cmd_full(uint8_t *buff, struct board_cmd_ *board_cmd, uint8_t *cmd_mode)
{
	uint16_t crc_recv, crc_calc;

	crc_recv = buff[32] << 8 | buff[33];
	crc_calc = crc16(buff, 32);
	if (crc_recv != crc_calc)
		return 1;

	memcpy(cmd_mode, buff + 1, 1);
	switch (*cmd_mode) {
	case CMD_LENGTH:
		memcpy(board_cmd[0].length, buff + 2, 8);
		memcpy(board_cmd[1].length, buff + 10, 8);
		memcpy(board_cmd[2].length, buff + 18, 8);
		break;
	case CMD_VALVE:
		memcpy(board_cmd[0].valve, buff + 2, 8);
		memcpy(board_cmd[0].DCV, buff + 10, 2);
		memcpy(board_cmd[1].valve, buff + 12, 8);
		memcpy(board_cmd[1].DCV, buff + 20, 2);
		memcpy(board_cmd[2].valve, buff + 22, 8);
		memcpy(board_cmd[2].DCV, buff + 30, 2);
	}

	return 0;
}

int get_packet(uint8_t *buff, int *got_startb, int (*getchar)(void), int n, int buff_size)
{
	int i;

	if (!*got_startb) {
		for (i = 0; i < n; i++, n--)
			if (getchar() == START_BYTE) {
				*got_startb = 1;
				break;
			}
	}

	if (!*got_startb || n < buff_size)
		return 1;

	buff[0] = START_BYTE;
	for (i = 1; i < buff_size; i++)
		buff[i] = getchar();
	*got_startb = 0;

	return 0;
}

