#include <stdio.h>
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

#define COMMS_PERIOD 20000

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

struct thread_data_ {
	struct cyl_ cyl_cmd[6];
	int curs_x, curs_y;
	pthread_mutex_t mutex;
};

static jmp_buf jump_buff;

void sigint_handler(int empty)
{
	longjmp(jump_buff, 1);
}

void pack_board_cmd(struct cyl_ *cyl, struct board_cmd_ *bcmd)
{
	int box = 0;
	int j = 0;
	int i;

	for (i = 0; i < 6; i++) {
		bcmd[box].length[j] = cyl[i].length;
		j = !j;
		if (!j)
			box++;
		//valve1
		//valve2
		//DCV
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
		cyl[i].valve1 = 0;
		cyl[i].valve2 = 0;
		cyl[i].DCV = 0;
		j = !j;
		if (!j)
			box++;
	}
}

void print_DCV(int y, int x, int pos)
{
	switch (pos) {
	case -1:
		mvprintw(y, x, "  RET");
		break;
	case 0:
		mvprintw(y, x, "CLOSE");
		break;
	case 1:
		mvprintw(y, x, "  EXT");
	}
}

void print_cylinder(int y, int x, int index, struct cyl_ *cyl, struct cyl_ *cyl_cmd)
{
	mvprintw(y + 0, x + 2, "Cylinder %d", index + 1);
	mvprintw(y + 1, x + 3, "cmd  state");
	mvprintw(y + 2, x + 0, "%6.2f %6.2f", cyl_cmd[index].length, cyl[index].length);
	mvprintw(y + 3, x + 0, "       %6.2f", cyl[index].press2 * PA_TO_PSI);
	mvprintw(y + 4, x + 0, "       %6.2f", cyl[index].press1 * PA_TO_PSI);
	mvprintw(y + 5, x + 0, "%5.1f  %5.1f", cyl_cmd[index].valve2 * 0.2, cyl[index].valve2 * 0.2);
	mvprintw(y + 6, x + 0, "%5.1f  %5.1f", cyl_cmd[index].valve1 * 0.2, cyl[index].valve1 * 0.2);
	print_DCV(y + 7, x + 0, cyl_cmd[index].DCV);
	print_DCV(y + 7, x + 7, cyl[index].DCV);
}

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

void *input_thread(void *data)
{
	struct thread_data_ *thread_data;
	int curs_cyl = 0;
	int curs_mode = 0;
	const int curs_offx[] = {18, 32, 46, 60, 74, 88, 104};
	const int curs_offy[] = {2, 5, 6, 7};
	struct cmd_adj_ cmd[6][4];
	int edit = 0;
	int i;

	thread_data = (struct thread_data_ *)data;

	for (i = 0; i < 6; i++) {
		cmd[i][0].target = &thread_data->cyl_cmd[i].length;
		cmd[i][0].min.f = 0.0;
		cmd[i][0].max.f = 30.0;
		cmd[i][0].inc.f = 1.0;
		cmd[i][0].type = TYPE_FLOAT;
		cmd[i][1].target = &thread_data->cyl_cmd[i].valve2;
		cmd[i][1].min.i = 0;
		cmd[i][1].max.i = 500;
		cmd[i][1].inc.i = 5;
		cmd[i][1].type = TYPE_INT;
		cmd[i][2].target = &thread_data->cyl_cmd[i].valve1;
		cmd[i][2].min.i = 0;
		cmd[i][2].max.i = 500;
		cmd[i][2].inc.i = 5;
		cmd[i][2].type = TYPE_INT;
		cmd[i][3].target = &thread_data->cyl_cmd[i].DCV;
		cmd[i][3].min.i = -1;
		cmd[i][3].max.i = 1;
		cmd[i][3].inc.i = 1;
		cmd[i][3].type = TYPE_INT;
	}

	pthread_mutex_lock(&thread_data->mutex);
	thread_data->curs_x = curs_offx[curs_cyl];
	thread_data->curs_y = curs_offy[curs_mode];
	pthread_mutex_unlock(&thread_data->mutex);

	while (1) {
		int c;

		c = getch();
		switch (c) {
		case 10:
			edit = 1;
			break;
		case 27:
			edit = 0;
			break;
		case 'h':
		case KEY_LEFT:
			if (curs_cyl > 0)
				curs_cyl--;
			else
				curs_cyl = 6;
			pthread_mutex_lock(&thread_data->mutex);
			thread_data->curs_x = curs_offx[curs_cyl];
			pthread_mutex_unlock(&thread_data->mutex);
			edit = 0;
			break;
		case 'l':
		case KEY_RIGHT:
			if (curs_cyl < 6)
				curs_cyl++;
			else
				curs_cyl = 0;
			pthread_mutex_lock(&thread_data->mutex);
			thread_data->curs_x = curs_offx[curs_cyl];
			pthread_mutex_unlock(&thread_data->mutex);
			edit = 0;
			break;
		case 'k':
		case KEY_UP:
			pthread_mutex_lock(&thread_data->mutex);
			if (edit) {
				if (curs_cyl < 6)
					cmd_adj(cmd[curs_cyl][curs_mode], 1);
				else
					for (i = 0; i < 6; i++)
						cmd_adj(cmd[i][curs_mode], 1);
			} else {
				if (curs_mode > 0)
					curs_mode--;
				else
					curs_mode = 3;
				thread_data->curs_y = curs_offy[curs_mode];
			}
			pthread_mutex_unlock(&thread_data->mutex);
			break;
		case 'j':
		case KEY_DOWN:
			pthread_mutex_lock(&thread_data->mutex);
			if (edit) {
				if (curs_cyl < 6)
					cmd_adj(cmd[curs_cyl][curs_mode], -1);
				else
					for (i = 0; i < 6; i++)
						cmd_adj(cmd[i][curs_mode], -1);
			} else {
				if (curs_mode < 3)
					curs_mode++;
				else
					curs_mode = 0;
				thread_data->curs_y = curs_offy[curs_mode];
			}
			pthread_mutex_unlock(&thread_data->mutex);
			break;
		}
	}
}

int main(void)
{
	struct cyl_ cyl[6];
	struct board_cmd_ bcmd[3];
	struct board_state_ bstate[3];
	struct thread_data_ thread_data;
	pthread_t input_thread_;
	int got_startb = 0;
	WINDOW *win;
	int i, j;

	if (setjmp(jump_buff))
		goto quit;

	signal(SIGINT, sigint_handler);

	memset(bstate, 0, 3 * sizeof(*bstate));
	memset(bcmd, 0, 3 * sizeof(*bcmd));
	memset(&thread_data, 0, sizeof(thread_data));

	if (comms_UART_init())
		return 1;

	if (pthread_mutex_init(&thread_data.mutex, NULL)) { 
		fprintf(stderr, "pthread_mutex_init() failed.\n"); 
		return 1; 
	} 

	if (pthread_create(&input_thread_, NULL, input_thread, &thread_data)) {
		fprintf(stderr, "pthread_create() failed.\n"); 
		return 1; 
	}

	if (!(win = initscr())) {
		fprintf(stderr, "initscr() failed.\n");
		return 1;
	}
	keypad(win, 1);

	cbreak();
	noecho();

	while (1) {
		pthread_mutex_lock(&thread_data.mutex);
		pack_board_cmd(thread_data.cyl_cmd, bcmd);
		pthread_mutex_unlock(&thread_data.mutex);
		comms_UART_send_cmd(bcmd);

		usleep(COMMS_PERIOD);

		comms_UART_recv_state(bstate, &got_startb);
		unpack_board_state(cyl, bstate);

		mvprintw(2, 0, "    length (in) 10");
		mvprintw(3, 0, "  p-upper (psi)");
		mvprintw(4, 0, "  p-lower (psi)");
		mvprintw(5, 0, "valve upper (%)");
		mvprintw(6, 0, "valve lower (%)");
		mvprintw(7, 0, "            DCV");
		pthread_mutex_lock(&thread_data.mutex);
		for (i = 0; i < 6; i++)
			print_cylinder(0, 16 + i * 14, i, cyl, thread_data.cyl_cmd);
		pthread_mutex_unlock(&thread_data.mutex);

		mvprintw(2, 104, "cmd-all");
		mvprintw(5, 104, "cmd-all");
		mvprintw(6, 104, "cmd-all");
		mvprintw(7, 104, "cmd-all");

		pthread_mutex_lock(&thread_data.mutex);
		move(thread_data.curs_y, thread_data.curs_x);
		pthread_mutex_unlock(&thread_data.mutex);

		refresh();
	}

quit:
	delwin(win);
	endwin();
	refresh();

	return 0;
}

