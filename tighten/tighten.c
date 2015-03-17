#include <stdio.h>

#include <signal.h>
#include <setjmp.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

static void *pruDataMem;
static unsigned int *pruDataMem_int;

static jmp_buf buf;

void signalCatcher (int null)
{
	longjmp (buf, 1);
}

int pruInit (void)
{
	tpruss_intc_initdata intc = PRUSS_INTC_INITDATA;

	if (prussdrv_init ()) {
		fprintf (stderr, "prussdrw_init () failed\n");
		return 1;
	}

	if (prussdrv_open (PRU_EVTOUT_1)) {
		fprintf (stderr, "prussdrv_open (PRU_EVTOUT_1) failed\n");
		return 1;
	}

	return 0;
}


int pruAbscond (void)
{
	if (prussdrv_pru_disable (1)) {
		fprintf (stderr, "prussdrv_pru_disable (1) failed\n");
		return 1;
	}

	if (prussdrv_exit()) {
		fprintf (stderr, "prussdrv_exit () failed\n");
		return 1;
	}

	return 0;
}

int main (int argc, char **argv)
{
	struct timespec waitTime = {0, 1000000000};
	struct timespec stepTime = {0, 10000000};
	int motorNum;
	int lastVal = 999;
	int setPos;
	int i;

	if (setjmp (buf))
		goto shutdown;

	signal (SIGINT, signalCatcher);

	if (argc != 2) {
		puts ("Need to pass one parameter, the number of the motor to be tightened.");
		return 1;
	}

	motorNum = atoi (argv[1]) - 1;

	if (motorNum > 11 || motorNum < 0) {
		puts ("Program expects a number between 1 and 12 as input");
		return 1;
	}

	pruInit ();

	prussdrv_map_prumem (PRUSS0_PRU1_DATARAM, &pruDataMem);
	pruDataMem_int = (unsigned int*) pruDataMem;

	for (i = 0; i < 12; i++)
		pruDataMem_int[i] = 0;

	if (prussdrv_exec_program (1, "./stepper.bin")) {
		fprintf (stderr, "prussdrv_exec_program (1, './stepper.bin') failed\n");
		return 1;
	}

	pruDataMem_int[motorNum] = 9999;

	nanosleep (&waitTime, NULL);

	while (lastVal != pruDataMem_int[91 + motorNum]) {
		lastVal = pruDataMem_int[91 + motorNum];
		nanosleep (&stepTime, NULL);
		printf ("%d\n", pruDataMem_int[91 + motorNum]);
	}

	setPos = (abs (pruDataMem_int[91 + motorNum]) >> 1) - 5;

shutdown:

	for (i = 0; i < 12; i++)
		pruDataMem_int[i] = 0;

	pruDataMem_int[motorNum] = setPos;

	while ((abs (pruDataMem_int[91 + motorNum]) >> 1) != setPos)
		printf ("%d\n", pruDataMem_int[91 + motorNum]);

	pruAbscond ();

	return 0;
}

