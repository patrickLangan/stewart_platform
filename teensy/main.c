#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "WProgram.h"
#include "pins_arduino.h"
#include "HardwareSerial.h"

#include "common.h"
#include "comms_UART.h"
#include "comms_485.h"
#include "util.h"
#include "spi.h"
#include "i2c.h"
#include "valve.h"
#include "control.h"

#define PRESSURE_PERIOD_4  2500
#define PRESSURE_PSHIFT       0
#define LSENSOR_PERIOD    10000
#define LSENSOR_PERIOD_2   5000
#define LSENSOR_PSHIFT     1000
#define COMMS_PERIOD_4     5000
#define COMMS_SLAVE_PERIOD 2000
#define COMMS_PSHIFT        500

#if BOARD == TEENSY1
const float length_scale[2] = {0.011859, 0.011839};
const float length_offset[2] = {-11.89, -12.04};
#elif BOARD == TEENSY2
const float length_scale[2] = {0.011889, 0.011833};
const float length_offset[2] = {-12.16, -12.01};
#elif BOARD == TEENSY3
const float length_scale[2] = {0.011866, 0.011786};
const float length_offset[2] = {-11.88, -11.9};
#endif

const int pressure_channel[4] = {3, 2, 1, 0};

/*
 * Re-package valve data into a more convenient format for board interface
 * (bstate) as opposed to direct valve interface (valve, DCV).
 */
void pack_valve_state(struct valve_ *valve, struct DCV_ *DCV, struct board_state_ *bstate)
{
	int i;
	for (i = 0; i < 4; i++)
		bstate->valve[i] = valve[i].pos;
	for (i = 0; i < 2; i++)
		bstate->DCV[i] = DCV[i].pos;
}

void unpack_valve_cmd(struct valve_ *valve, struct DCV_ *DCV, struct board_cmd_ *bcmd)
{
	int i;
	for (i = 0; i < 4; i++)
		valve[i].cmd = bcmd->valve[i];
	for (i = 0; i < 2; i++)
		DCV[i].cmd = bcmd->DCV[i];
}

/*
 * Read I2C pressure sensor, convert raw data to pascals.
 */
static int pressure_read(int channel, float *val)
{
	const float pressure_scale = 63.1272415218;
	uint8_t buff[2];
	uint16_t tmp;

	if (i2c_read(buff, channel, 0x28, 2))
		return 1;

	tmp = (uint16_t)buff[0] << 8 | (uint16_t)buff[1];
	*val = (float)tmp * pressure_scale;

	return 0;
}

/*
 * Read SPI length sensor ADC, convert raw data to meters.
 */
static int length_read(int channel, float *val, float scale, float offset)
{
	uint8_t buff[2];
	uint16_t tmp;

	spi_read(buff, channel, 2);

	tmp = (((buff[0] << (int32_t)8) | buff[1]) >> 1) & 0xFFF;
	*val = (float)tmp * (scale * IN_TO_M) + (offset * IN_TO_M);

	return 0;
}

/*
 * Manage timing of when sensor readings are taken.  Read one at a time to
 * avoid taking up too much time per call (single thread).
 */
void pressure_advance(float *pressure)
{
	const int sensor_num = 4;
	static int i = sensor_num - 1;
	static uint32_t timer = PRESSURE_PSHIFT;

	if (limit_frequency(micros(), &timer, PRESSURE_PERIOD_4))
		return;

	pressure_read(pressure_channel[i], &pressure[i]);
	if (--i < 0)
		i = sensor_num - 1;
}

void length_advance(float *length, float *length_dot)
{
	static int first[2] = {1, 1};
	float last_length;
	static int i;
	static uint32_t timer = LSENSOR_PSHIFT;

	if (limit_frequency(micros(), &timer, LSENSOR_PERIOD_2))
		return;

	last_length = length[i];
	length_read(i, &length[i], length_scale[i], length_offset[i]);

	if (first[i]) {
		first[i] = 0;
		last_length = length[i];
	}

	length_dot[i] = (length[i] - last_length) / (LSENSOR_PERIOD / 1e6);

	i = !i;
}

/*
 * Manage timing of comms packet transmission/reception.
 */
void comms_advance(struct board_state_ *bstate, struct board_cmd_ *bcmd, uint8_t *cmd_mode, struct valve_ *valve, struct DCV_ *DCV)
{
	static uint32_t timer = COMMS_PSHIFT;
#if BOARD == TEENSY1
	static int got_startb;
	static int state;

	if (limit_frequency(micros(), &timer, COMMS_PERIOD_4))
		return;

	switch (state) {
	default:
	case 0:
		while (!comms_UART_recv_cmd(bcmd, &got_startb, cmd_mode))
			;
		if (*cmd_mode == CMD_VALVE)
			unpack_valve_cmd(valve, DCV, &bcmd[0]);
		state++;
		break;
	case 1:
		comms_485_send(1, &bcmd[1], *cmd_mode);
		comms_485_recv(2, &bstate[2]);
		state++;
		break;
	case 2:
		pack_valve_state(valve, DCV, &bstate[0]);
		comms_UART_send_state(bstate);
		state++;
		break;
	case 3:
		comms_485_send(2, &bcmd[2], *cmd_mode);
		comms_485_recv(1, &bstate[1]);
		state = 0;
	}
#else
	if (limit_frequency(micros(), &timer, COMMS_SLAVE_PERIOD))
		return;

	if (!comms_485_slave(&bstate[BOARD], &bcmd[BOARD], cmd_mode)) {
		if (*cmd_mode)
			unpack_valve_cmd(valve, DCV, &bcmd[BOARD]);
		pack_valve_state(valve, DCV, &bstate[BOARD]);
	}
#endif
}

/*
 * Initiallize data, call initialization functions.
 * Call board communications, sensor interface, control algorithm, and valve
 * interface functions in a loop.
 */
int main(void)
{
	struct board_state_ bstate[3];
	struct board_cmd_ bcmd[3];
	struct valve_ valve[4];
	struct DCV_ DCV[4];
	struct control_data_ control_data[2];
	uint8_t cmd_mode;
	int i;

	memset(bstate, 0, 3 * sizeof(*bstate));
	memset(bcmd, 0, 3 * sizeof(*bcmd));
	memset(valve, 0, 4 * sizeof(*valve));
	memset(DCV, 0, 2 * sizeof(*DCV));
	memset(control_data, 0, 2 * sizeof(*control_data));

	comms_UART_init();
	comms_485_init();
	i2c_init(400000, 10000);
	spi_init();
	valve_init(valve, DCV);

	while (1) {
		comms_advance(bstate, bcmd, &cmd_mode, valve, DCV);

		pressure_advance(bstate[BOARD].pressure);
		length_advance(bstate[BOARD].length, bstate[BOARD].length_dot);

		switch (cmd_mode) {
		case CMD_LENGTH:
			for (i = 0; i < 2; i++) {
				control_advance(&control_data[i],
					bstate[BOARD].length[i],
					bstate[BOARD].length_dot[i],
					bstate[BOARD].pressure[i * 2 + 0],
					bstate[BOARD].pressure[i * 2 + 1],
					bcmd[BOARD].length[i],
					micros());

				valve_control_input(&valve[i * 2 + 0],
				                    &valve[i * 2 + 1],
				                    &DCV[i],
				                    control_data[i].x5,
						    control_data[i].x6);
			}
			break;
		case CMD_VALVE:
			for (i = 0; i < 2; i++)
				DCV_switch(&DCV[i]);
			for (i = 0; i < 4; i++)
				valve_step(&valve[i]);
		}
	}

	return 0;
}

