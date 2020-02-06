#include "comms_485.h"

#include "WProgram.h"
#include "pins_arduino.h"
#include "HardwareSerial.h"

#include "common.h"

void comms_485_init(void)
{
	serial3_set_rx(7);
	serial3_set_tx(8, 0);
	serial3_set_transmit_pin(16);
	serial3_begin(BAUD2DIV3(BAUD_));
	serial3_clear();
	serial3_format(SERIAL_8N1);

#if BOARD == TEENSY1
	serial5_set_rx(34);
	serial5_set_tx(33, 0);
	serial5_set_transmit_pin(39);
	serial5_begin(BAUD2DIV3(BAUD_));
	serial5_clear();
	serial5_format(SERIAL_8N1);
#endif
}

int comms_485_send(int index, struct board_cmd_ *board_cmd)
{
	uint8_t buff[SIZE_CMD] = {0};

	switch (index) {
	default:
	case 1:
		if (!serial3_write_buffer_free())
			return 1;
		pack_cmd(buff, board_cmd);
		serial3_write(buff, SIZE_CMD);
		break;
	case 2:
		if (!serial5_write_buffer_free())
			return 1;
		pack_cmd(buff, board_cmd);
		serial5_write(buff, SIZE_CMD);
	}

	return 0;
}

int comms_485_recv(int index, struct board_state_ *board_state, int *got_startb)
{
	uint8_t buff[SIZE_STATE] = {0};

	switch (index) {
	default:
	case 1:
		if (get_packet(buff, got_startb, serial3_getchar, serial3_available(), SIZE_STATE))
			return 1;
		break;
	case 2:
		if (get_packet(buff, got_startb, serial5_getchar, serial5_available(), SIZE_STATE))
			return 1;
	}

	unpack_state(buff, board_state);

	return 0;
}

int comms_485_slave(struct board_state_ *board_state, struct board_cmd_ *board_cmd, int *got_startb)
{
	uint8_t buff_tx[SIZE_STATE] = {0};
	uint8_t buff_rx[SIZE_CMD] = {0};

	if (!serial3_available())
		return 1;

	if (get_packet(buff_rx, got_startb, serial3_getchar, serial3_available(), SIZE_CMD))
		return 1;

	unpack_cmd(buff_rx, board_cmd);

	if (serial3_write_buffer_free()) {
		pack_state(buff_tx, board_state);
		serial3_write(buff_tx, SIZE_STATE);
		serial3_flush();
	}

	return 0;
}

