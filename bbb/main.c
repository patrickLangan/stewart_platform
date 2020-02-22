#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <math.h>
#include <pthread.h>
#include <curses.h>

#include "common.h"
#include "util.h"
#include "comms_UART.h"

enum type_ {
	TYPE_INT,
	TYPE_FLOAT
};

union flin {
	float f;
	float i;
};

struct cmd_adj_ {
	void *target;
	union flin min;
	union flin max;
	union flin inc;
	enum type_ type;
};

struct cyl_ {
	float length;
	float press1;
	float press2;
	int valve1;
	int valve2;
	int DCV;
};

struct data_ {
	struct cyl_ cyl_cmd[6];
	uint8_t cmd_mode;
	int edit;
	int curs_cyl, curs_mode;
	int curs_x, curs_y;
	int estop;
	int motion_file;
	int argc;
	char **argv;
	WINDOW *win;
	pthread_mutex_t mutex;
};

static jmp_buf jump_buff;

void sigint_handler(int empty)
{
	longjmp(jump_buff, 1);
}

double time_diff(struct timespec t2, struct timespec t1)
{
	return ((double)t2.tv_sec + (double)t2.tv_nsec / 1e9) - ((double)t1.tv_sec + (double)t1.tv_nsec / 1e9);
}

/*
 * Re-package cylinder data into a more convenient format for board interface
 * (bcmd) as opposed to HMI interface (cyl).
 */
void pack_board_cmd(struct cyl_ *cyl, struct board_cmd_ *bcmd)
{
	int box = 0;
	int j = 0;
	int i;

	for (i = 0; i < 6; i++) {
		bcmd[box].length[j] = cyl[i].length;
		bcmd[box].valve[j * 2 + 0] = cyl[i].valve1;
		bcmd[box].valve[j * 2 + 1] = cyl[i].valve2;
		bcmd[box].DCV[j] = cyl[i].DCV;
		j = !j;
		if (!j)
			box++;
	}
}

void unpack_board_state(struct cyl_ *cyl, struct board_state_ *bstate)
{
	int box = 0;
	int j = 0;
	int i;

	for (i = 0; i < 6; i++) {
		cyl[i].length = bstate[box].length[j];
		cyl[i].press1 = bstate[box].pressure[j * 2 + 0];
		cyl[i].press2 = bstate[box].pressure[j * 2 + 1];
		cyl[i].valve1 = bstate[box].valve[j * 2 + 0];
		cyl[i].valve2 = bstate[box].valve[j * 2 + 1];
		cyl[i].DCV = bstate[box].DCV[j];
		j = !j;
		if (!j)
			box++;
	}
}

/* Print string with ncurses color formatting */
void mvprint_colorf(int y, int x, const char *format, float val, int pair)
{
	if (pair) {
		attron(COLOR_PAIR(pair));
		mvprintw(y, x, format, val);
		attroff(COLOR_PAIR(pair));
		return;
	}
	mvprintw(y, x, format, val);
}

/* Print out Directional Control Valve (DCV) enum */
void print_DCV(int y, int x, enum DCV_pos_ pos, int pair)
{
	if (pair)
		attron(COLOR_PAIR(pair));

	switch (pos) {
	case DCV_RETRACT:
		mvprintw(y, x, "  RET");
		break;
	case DCV_CLOSE:
		mvprintw(y, x, "CLOSE");
		break;
	case DCV_EXTEND:
		mvprintw(y, x, "  EXT");
	}

	if (pair)
		attroff(COLOR_PAIR(pair));
}

void print_cylinder(int y, int x, int index, struct cyl_ *cyl, struct cyl_ *cyl_cmd, int edit, int curs_mode)
{
	mvprintw(y + 0, x + 2, "Cylinder %d", index + 1);
	mvprintw(y + 1, x + 3, "cmd  state");

	mvprint_colorf(y + 2, x + 0, "%6.2f", cyl_cmd[index].length * M_TO_IN, (edit && !curs_mode) ? 1 : 0);
	printw(" %6.2f", cyl[index].length * M_TO_IN);

	mvprintw(y + 3, x + 0, "       %6.2f", cyl[index].press2 * PA_TO_PSI);
	mvprintw(y + 4, x + 0, "       %6.2f", cyl[index].press1 * PA_TO_PSI);

	mvprint_colorf(y + 5, x + 0, "%5.1f", cyl_cmd[index].valve2 * STP_TO_PERCENT, (edit && curs_mode == 1) ? 1 : 0);
	printw("  %5.1f", cyl[index].valve2 * STP_TO_PERCENT);
	mvprint_colorf(y + 6, x + 0, "%5.1f", cyl_cmd[index].valve1 * STP_TO_PERCENT, (edit && curs_mode == 2) ? 1 : 0);
	printw("  %5.1f", cyl[index].valve1 * STP_TO_PERCENT);

	print_DCV(y + 7, x + 0, cyl_cmd[index].DCV, (edit && curs_mode == 3) ? 1 : 0);
	print_DCV(y + 7, x + 7, cyl[index].DCV, 0);
}

void *motion_thread(void *data_)
{
	struct data_ *data;

	int last_motion_file;
	int motion_file = 0;

	FILE *file = NULL;
	float f;

	struct timespec loop_time;
	struct timespec start_time;
	int cur_time = 0;
	int last_time;
	int prog_time;

	int step_num;
	int time_step;
	int loop_end;
	int loop_start;

	float set_length[6] = {0};

	int i = 0;
	int j;

	data = (struct data_ *)data_;

	while (1) {
		last_motion_file = motion_file;

		pthread_mutex_lock(&data->mutex);
		motion_file = data->motion_file;
		pthread_mutex_unlock(&data->mutex);

		if (!motion_file) {
			usleep(100000);
			continue;
		}

		if (motion_file != last_motion_file) {
			if (file)
				fclose(file);

			file = fopen(data->argv[motion_file], "r");

			if (!file) {
				fprintf(stderr, "Failed to open input file: %s\n", data->argv[motion_file]);
				sleep(1);
				motion_file = 0;
				continue;
			}

			fread(&f, sizeof(float), 1, file);
			step_num = (int)f;
			fread (&f, sizeof(float), 1, file);
			time_step = (int)f * 1000;
			fread(&f, sizeof(float), 1, file);
			loop_end = (int)f + 1;
			fread(&f, sizeof(float), 1, file);
			loop_start = (int)f;

			clock_gettime(CLOCK_REALTIME, &start_time);
		}

		if (i >= loop_end) {
			fseek(file, (4 + 6 * loop_start) * sizeof(float), SEEK_SET);
			clock_gettime(CLOCK_REALTIME, &start_time);
			i = loop_start;
			cur_time = 0;
			prog_time = time_step;
		}

		last_time = cur_time;
		clock_gettime(CLOCK_REALTIME, &loop_time);
		cur_time = (loop_time.tv_sec - start_time.tv_sec) * 1000000 + (loop_time.tv_nsec - start_time.tv_nsec) / 1000;
		if (cur_time > prog_time) {
			for (j = 0; j < 6; j++)
				fread(&set_length[j], sizeof(float), 1, file);

			pthread_mutex_lock(&data->mutex);
			for (j = 0; j < 6; j++)
				data->cyl_cmd[j].length = set_length[j] * IN_TO_M;
			pthread_mutex_unlock(&data->mutex);

			prog_time += time_step;
			i++;
		}
	}
}

/*
 * Set the variable pointed to by "cmd".  The utility of this interface is to
 * allow for the same function call regardless of type (int or float) of the
 * underlying data.
 */
void cmd_set(struct cmd_adj_ cmd, union flin val)
{
	switch (cmd.type) {
	case TYPE_INT:
		*(int *)cmd.target = val.i;
		break;
	case TYPE_FLOAT:
		*(float *)cmd.target = val.f;
	}
}

/*
 * Increment or decrement the variable pointed to by "cmd".
 */
void cmd_adj(struct cmd_adj_ cmd, int sign)
{
	switch (cmd.type) {
	case TYPE_INT:
		*(int *)cmd.target = ilimit(*(int *)cmd.target + (float)sign * cmd.inc.i, cmd.min.i, cmd.max.i);
		break;
	case TYPE_FLOAT:
		*(float *)cmd.target = flimit(*(float *)cmd.target + sign * cmd.inc.f, cmd.min.f, cmd.max.f);
	}
}

/*
 * Thread to manage keyboard input operations:
 * 	* Select a command type, and which cylinder(s) (one or all) are being
 * 	  commanded.
 *		* closed-loop cylinder length setpoint
 *		* manual valve command: stepper-valve1,2 or DCV
 * 	* Select a motion file
 *	* E-Stop
 */
void *input_thread(void *data_)
{
	struct data_ *data;
	const int curs_offx[] = {18, 32, 46, 60, 74, 88, 104};
	const int curs_offy[] = {2, 5, 6, 7};
	struct cmd_adj_ cmd[6][4];
	union flin cmd_zero = {0};
	int reading_n = 0;
	int read_n = 0;
	char read_n_buff[255];
	int i, j;
	int ret = 0;

	data = (struct data_ *)data_;

	for (i = 0; i < 6; i++) {
		cmd[i][0].target = &data->cyl_cmd[i].length;
		cmd[i][0].min.f = 0.0;
		cmd[i][0].max.f = 0.762;
		cmd[i][0].inc.f = 1.0 * IN_TO_M;
		cmd[i][0].type = TYPE_FLOAT;
		cmd[i][1].target = &data->cyl_cmd[i].valve2;
		cmd[i][1].min.i = -10;
		cmd[i][1].max.i = STP_MAX;
		cmd[i][1].inc.i = 10;
		cmd[i][1].type = TYPE_INT;
		cmd[i][2].target = &data->cyl_cmd[i].valve1;
		cmd[i][2].min.i = -10;
		cmd[i][2].max.i = STP_MAX;
		cmd[i][2].inc.i = 10;
		cmd[i][2].type = TYPE_INT;
		cmd[i][3].target = &data->cyl_cmd[i].DCV;
		cmd[i][3].min.i = DCV_RETRACT;
		cmd[i][3].max.i = DCV_EXTEND;
		cmd[i][3].inc.i = 1;
		cmd[i][3].type = TYPE_INT;
	}

	pthread_mutex_lock(&data->mutex);
	data->curs_x = curs_offx[data->curs_cyl];
	data->curs_y = curs_offy[data->curs_mode];
	pthread_mutex_unlock(&data->mutex);

	while (1) {
		int c;
		int key_updown = 0;

		c = getch();

		if (reading_n) {
			if (c >= '0' && c <= '9') {
				read_n_buff[read_n++] = c;
				continue;
			}
			if (c == 10 && read_n) {
				int n;
				read_n_buff[read_n] = '\0';
				n = atoi(read_n_buff);
				if (n < data->argc) {
					pthread_mutex_lock(&data->mutex);
					data->motion_file = n;
					data->cmd_mode = CMD_LENGTH;
					pthread_mutex_unlock(&data->mutex);
				}
			}
			read_n = 0;
			reading_n = 0;
			continue;
		}

		pthread_mutex_lock(&data->mutex);

		switch (c) {
		case ' ':
			for (i = 0; i < 6; i++)
				for (j = 0; j < 4; j++)
					cmd_set(cmd[i][j], cmd_zero);
			data->edit = 0;
			data->estop = 1;
			data->cmd_mode = CMD_VALVE;
			data->motion_file = 0;
			curs_set(0);
			pthread_mutex_unlock(&data->mutex);
			pthread_exit(&ret);
			break;
		case 10:
			data->edit = !data->edit;
			break;
		case 27:
			data->edit = 0;
			break;
		case 'h':
		case KEY_LEFT:
			if (data->curs_cyl > 0)
				data->curs_cyl--;
			else
				data->curs_cyl = 6;
			data->curs_x = curs_offx[data->curs_cyl];
			data->edit = 0;
			break;
		case 'l':
		case KEY_RIGHT:
			if (data->curs_cyl < 6)
				data->curs_cyl++;
			else
				data->curs_cyl = 0;
			data->curs_x = curs_offx[data->curs_cyl];
			data->edit = 0;
			break;
		case 'k':
		case KEY_UP:
			key_updown = 1;
			if (!data->edit) {
				if (data->curs_mode > 0)
					data->curs_mode--;
				else
					data->curs_mode = 3;
			}
			break;
		case 'j':
		case KEY_DOWN:
			key_updown = -1;
			if (!data->edit) {
				if (data->curs_mode < 3)
					data->curs_mode++;
				else
					data->curs_mode = 0;
			}
			break;
		case 'r':
			reading_n = 1;
		}

		if (key_updown) {
			if (data->edit) {
				if (data->curs_cyl < 6)
					cmd_adj(cmd[data->curs_cyl][data->curs_mode], key_updown);
				else
					for (i = 0; i < 6; i++)
						cmd_adj(cmd[i][data->curs_mode], key_updown);
				data->motion_file = 0;
				if (!data->curs_mode)
					data->cmd_mode = CMD_LENGTH;
				else
					data->cmd_mode = CMD_VALVE;
			} else {
				data->curs_y = curs_offy[data->curs_mode];
			}
		}

		pthread_mutex_unlock(&data->mutex);
	}
}

/*
 * Initiallize data, call initialization functions.
 * Draw screen and handle off-board microcontroller I/O in a loop.
 */
int main(int argc, char **argv)
{
	struct cyl_ cyl[6];
	struct board_cmd_ bcmd[3];
	struct board_state_ bstate[3];
	struct data_ data;
	uint8_t cmd_mode;
	int motion_file;
	pthread_t motion_thread_;
	pthread_t input_thread_;
	int got_startb = 0;
	struct timespec last_time;
	struct timespec cur_time;
	int i, j;

	if (setjmp(jump_buff))
		goto quit;

	signal(SIGINT, sigint_handler);

	memset(bstate, 0, 3 * sizeof(*bstate));
	memset(bcmd, 0, 3 * sizeof(*bcmd));
	memset(&data, 0, sizeof(data));

	data.cmd_mode = CMD_VALVE;
	data.argc = argc;
	data.argv = argv;

	if (comms_UART_init())
		return 1;

	if (pthread_mutex_init(&data.mutex, NULL)) {
		fprintf(stderr, "pthread_mutex_init() failed.\n"); 
		return 1; 
	} 

	if (pthread_create(&motion_thread_, NULL, motion_thread, &data)) {
		fprintf(stderr, "pthread_create() failed.\n");
		return 1;
	}

	if (pthread_create(&input_thread_, NULL, input_thread, &data)) {
		fprintf(stderr, "pthread_create() failed.\n");
		return 1;
	}

	if (!(data.win = initscr())) {
		fprintf(stderr, "initscr() failed.\n");
		return 1;
	}
	keypad(data.win, 1);

	start_color();
	init_pair(1, COLOR_BLACK, COLOR_WHITE);
	init_pair(2, COLOR_BLACK, COLOR_RED);
	init_pair(3, COLOR_RED, COLOR_BLACK);

	cbreak();
	noecho();

	clock_gettime(CLOCK_REALTIME, &last_time);

	while (1) {
		while (!comms_UART_recv_state(bstate, &got_startb))
			;
		while (comms_UART_recv_state(bstate, &got_startb))
			;
		unpack_board_state(cyl, bstate);

		clock_gettime(CLOCK_REALTIME, &cur_time);

		pthread_mutex_lock(&data.mutex);
		pack_board_cmd(data.cyl_cmd, bcmd);
		cmd_mode = data.cmd_mode;
		motion_file = data.motion_file;
		pthread_mutex_unlock(&data.mutex);
		comms_UART_send_cmd(bcmd, cmd_mode);

		clear();

		mvprintw(2, 0, "    length (in)");
		mvprintw(3, 0, "  p-upper (psi)");
		mvprintw(4, 0, "  p-lower (psi)");
		mvprintw(5, 0, "valve upper (%)");
		mvprintw(6, 0, "valve lower (%)");
		mvprintw(7, 0, "            DCV");
		pthread_mutex_lock(&data.mutex);
		for (i = 0; i < 6; i++) {
			int edit = 0;
			if (data.edit && (i == data.curs_cyl || data.curs_cyl == 6))
				edit = 1;
			print_cylinder(0, 16 + i * 14, i, cyl, data.cyl_cmd, edit, data.curs_mode);
		}
		pthread_mutex_unlock(&data.mutex);

		mvprintw(2, 104, "cmd-all");
		mvprintw(5, 104, "cmd-all");
		mvprintw(6, 104, "cmd-all");
		mvprintw(7, 104, "cmd-all");

		mvprintw(9, 0, "Motion files:");
		for (i = 1; i < argc; i++) {
			if (i == motion_file)
				attron(COLOR_PAIR(1));
			mvprintw(9 + i, 0, "%3d %s", i, argv[i]);
			if (i == motion_file)
				attroff(COLOR_PAIR(1));
		}

		if (data.estop) {
			static int pair = 2;
			if (time_diff(cur_time, last_time) > 0.25) {
				last_time = cur_time;
				pair = (pair == 2) ? 3 : 2;
			}
			attron(COLOR_PAIR(pair));
			mvprintw(9, 47, "E-STOP - restart required");
			attroff(COLOR_PAIR(pair));
		} else {
			attron(COLOR_PAIR(2));
			mvprintw(9, 50, "E-STOP (SPACE BAR)");
			attroff(COLOR_PAIR(2));
		}

		mvprintw(9, 80, "cmd-mode: ");
		if (motion_file)
			printw("MOTION FILE");
		else if (cmd_mode == CMD_LENGTH)
			printw("LENGTH");
		else if (cmd_mode == CMD_VALVE)
			printw("VALVE");

		pthread_mutex_lock(&data.mutex);
		move(data.curs_y, data.curs_x);
		pthread_mutex_unlock(&data.mutex);

		refresh();
	}

quit:
	delwin(data.win);
	endwin();
	refresh();

	return 0;
}

