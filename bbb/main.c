#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "comms_UART.h"

#define COMMS_PERIOD 20000

#define LOOP_TEST 0

const int cyl_box[6] = {0, 0, 1, 1, 2, 2};
const int cyl_j[6] = {0, 1, 0, 1, 0, 1};

float cmd;
struct board_cmd_ bcmd[3];

void *input_thread(void *data)
{
	int i;

	while (1) {
		int index;
		float cmd;

		scanf("%d", &index);
		scanf("%f", &cmd);

		index--;
		if (index >= 0 && index < 6) {
			bcmd[cyl_box[index]].length[cyl_j[index]] = cmd;
			continue;
		}
		for (i = 0; i < 3; i++) {
			bcmd[i].length[0] = cmd;
			bcmd[i].length[1] = cmd;
		}
	}
}

int main(void)
{
	struct board_state_ bstate[3];
	int got_startb = 0;
	pthread_t input_thread_;
	int i, j;

#if LOOP_TEST
	int up = 1;
	int loop = 0;
	int cylinder = -1;
#endif

	memset(bstate, 0, 3 * sizeof(*bstate));
	memset(bcmd, 0, 3 * sizeof(*bcmd));

	comms_UART_init();

	pthread_create(&input_thread_, NULL, input_thread, NULL);

	while (1) {
#if LOOP_TEST
		if (loop <= 0) {
			float cmd;
			loop = 51;
			cylinder++;
			if (cylinder > 5) {
				cylinder = 0;
				up = !up;
			}
			if (up)
				cmd = 10.0;
			else
				cmd = -10.0;
			for (i = 0; i < 3; i++) {
				bcmd[i].length[0] = 0;
				bcmd[i].length[1] = 0;
			}
			bcmd[cyl_box[cylinder]].length[cyl_j[cylinder]] = cmd;
		}
		loop--;
#endif

		comms_UART_send_cmd(bcmd);

		usleep(COMMS_PERIOD);

		comms_UART_recv_state(bstate, &got_startb);

		for (i = 0; i < 3; i++)
			for (j = 0; j < 4; j++)
				printf("%0.2f\t", bstate[i].pressure[j] * 0.0001450377);
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++)
				printf("%0.2f\t", bstate[i].length[j]);
		puts("");
	}

	return 0;
}

