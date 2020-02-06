#include "comms_UART.h"

#if BOARD == BBB
#include <stdio.h>
#include <fcntl.h> 
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#else
#include "WProgram.h"
#include "pins_arduino.h"
#include "HardwareSerial.h"
#endif

#if BOARD == BBB
int serial_fd;
#endif

#if BOARD == BBB
static int unix_getchar(void)
{
	uint8_t ret;
	read(serial_fd, &ret, 1);
	return ret;
}

/* https://stackoverflow.com/a/6947758 */
static int set_interface_attribs(int fd, int speed, int parity, int should_block)
{
	struct termios tty;

	if (tcgetattr(fd, &tty)) {
		fprintf(stderr, "tcgetattr() failed.");
		return 1;
	}

	cfsetospeed(&tty, speed);
	cfsetispeed(&tty, speed);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
	tty.c_iflag &= ~IGNBRK;
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cc[VMIN]  = !!should_block;
	tty.c_cc[VTIME] = 1;
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);
	tty.c_cflag |= (CLOCAL | CREAD);
	tty.c_cflag &= ~(PARENB | PARODD);
	tty.c_cflag |= parity;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr(fd, TCSANOW, &tty)) {
		fprintf(stderr, "tcsetattr() failed.");
		return 1;
	}

	return 0;
}
#endif

void comms_UART_init(void)
{
#if BOARD == BBB
	serial_fd = open("/dev/ttyO4", O_RDWR | O_NOCTTY | O_SYNC);
	set_interface_attribs(serial_fd, B115200, 0, 0);
#else
	serial_set_rx(21);
	serial_set_tx(5, 0);
	serial_begin(BAUD2DIV(BAUD_));
	serial_clear();
	serial_format(SERIAL_8N1);
#endif
}

#if BOARD == BBB
int comms_UART_send_cmd(struct board_cmd_ *board_cmd)
{
	uint8_t buff[SIZE_CMD_FULL] = {0};

	tcflush(serial_fd, TCIOFLUSH);

	pack_cmd_full(buff, board_cmd);
	write(serial_fd, buff, SIZE_CMD_FULL);

	tcflush(serial_fd, TCIOFLUSH);

	return 0;
}

int comms_UART_recv_state(struct board_state_ *board_state, int *got_startb)
{
	uint8_t buff[SIZE_STATE_FULL] = {0};
	int n;

	ioctl(serial_fd, FIONREAD, &n);
	if (get_packet(buff, got_startb, unix_getchar, n, SIZE_STATE_FULL))
		return 1;

	unpack_state_full(buff, board_state);

	return 0;
}
#else
int comms_UART_recv_cmd(struct board_cmd_ *board_cmd, int *got_startb)
{
	uint8_t buff[SIZE_CMD_FULL] = {0};

	if (!serial_available())
		return 1;

	if (get_packet(buff, got_startb, serial_getchar, serial_available(), SIZE_CMD_FULL))
		return 1;

	unpack_cmd_full(buff, board_cmd);

	return 0;
}

int comms_UART_send_state(struct board_state_ *board_state)
{
	uint8_t buff[SIZE_STATE_FULL] = {0};

	if (!serial_write_buffer_free())
		return 1;

	pack_state_full(buff, board_state);
	serial_write(buff, SIZE_STATE_FULL);

	return 0;
}
#endif

