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
	memcpy(buff + 1, board_state->pressure, 16);
	memcpy(buff + 17, board_state->length, 8);
	crc = crc16(buff, 25);
	buff[25] = crc >> 8;
	buff[26] = crc & 0xFF;
}

int unpack_state(uint8_t *buff, struct board_state_ *board_state)
{
	uint16_t crc_recv, crc_calc;

	crc_recv = buff[25] << 8 | buff[26];
	crc_calc = crc16(buff, 25);
	if (crc_recv != crc_calc)
		return 1;

	memcpy(board_state->pressure, buff + 1, 16);
	memcpy(board_state->length, buff + 17, 8);

	return 0;
}

void pack_state_full(uint8_t *buff, struct board_state_ *board_state)
{
	uint16_t crc;
	buff[0] = START_BYTE;
	memcpy(buff + 1, board_state[0].pressure, 16);
	memcpy(buff + 17, board_state[0].length, 8);
	memcpy(buff + 25, board_state[1].pressure, 16);
	memcpy(buff + 41, board_state[1].length, 8);
	memcpy(buff + 49, board_state[2].pressure, 16);
	memcpy(buff + 65, board_state[2].length, 8);
	crc = crc16(buff, 73);
	buff[73] = crc >> 8;
	buff[74] = crc & 0xFF;
}

int unpack_state_full(uint8_t *buff, struct board_state_ *board_state)
{
	uint16_t crc_recv, crc_calc;

	crc_recv = buff[73] << 8 | buff[74];
	crc_calc = crc16(buff, 73);
	if (crc_recv != crc_calc)
		return 1;

	memcpy(board_state[0].pressure, buff + 1, 16);
	memcpy(board_state[0].length, buff + 17, 8);
	memcpy(board_state[1].pressure, buff + 25, 16);
	memcpy(board_state[1].length, buff + 41, 8);
	memcpy(board_state[2].pressure, buff + 49, 16);
	memcpy(board_state[2].length, buff + 65, 8);

	return 0;
}

void pack_cmd(uint8_t *buff, struct board_cmd_ *board_cmd)
{
	uint16_t crc;
	buff[0] = START_BYTE;
	memcpy(buff + 1, board_cmd->length, 8);
	crc = crc16(buff, 9);
	buff[9]  = crc >> 8;
	buff[10] = crc & 0xFF;
}

int unpack_cmd(uint8_t *buff, struct board_cmd_ *board_cmd)
{
	uint16_t crc_recv, crc_calc;

	crc_recv = buff[9] << 8 | buff[10];
	crc_calc = crc16(buff, 9);
	if (crc_recv != crc_calc)
		return 1;

	memcpy(board_cmd->length, buff + 1, 8);

	return 0;
}

void pack_cmd_full(uint8_t *buff, struct board_cmd_ *board_cmd)
{
	uint16_t crc;
	buff[0] = START_BYTE;
	memcpy(buff + 1, board_cmd[0].length, 8);
	memcpy(buff + 9, board_cmd[1].length, 8);
	memcpy(buff + 17, board_cmd[2].length, 8);
	crc = crc16(buff, 25);
	buff[25]  = crc >> 8;
	buff[26] = crc & 0xFF;
}

int unpack_cmd_full(uint8_t *buff, struct board_cmd_ *board_cmd)
{
	uint16_t crc_recv, crc_calc;

	crc_recv = buff[25] << 8 | buff[26];
	crc_calc = crc16(buff, 25);
	if (crc_recv != crc_calc)
		return 1;

	memcpy(board_cmd[0].length, buff + 1, 8);
	memcpy(board_cmd[1].length, buff + 9, 8);
	memcpy(board_cmd[2].length, buff + 17, 8);

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

