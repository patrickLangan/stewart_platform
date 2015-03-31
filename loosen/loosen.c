#include <stdio.h>

#include <signal.h>
#include <setjmp.h>

#include <prussdrv.h>
#include <pruss_intc_mapping.h>

static void *pruDataMem;
static unsigned int *pruDataMem_int;

static jmp_buf buf;

struct gpioInfo {
        int pin;
        int value;
        FILE *file;
};

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


int gpioOutputInit (struct gpioInfo *gpio, char *value)
{
        char gpioPath[30];

        sprintf (gpioPath, "/sys/class/gpio/gpio%d/value", gpio->pin);
        gpio->file = fopen (gpioPath, "w");
        fprintf (gpio->file, value);
        fflush (gpio->file);

        return 0;
}

int gpioOutputAbscond (struct gpioInfo *gpio, char *value)
{
        fprintf (gpio->file, value);
        fclose (gpio->file);
        return 0;
}

int main (int argc, char **argv)
{
	struct gpioInfo controlValve[6] = {{65}, {66}, {67}, {68}, {69}, {12}};
	int i, j;

	if (setjmp (buf))
		goto shutdown;

	signal (SIGINT, signalCatcher);

	if (argc != 4) {
		printf ("Need to pass three parameters.\n");
		printf ("The first is the amount to open the valve.\n");
		printf ("The second is the final position to leave the valve.\n");
		printf ("The third is the direction in which to set the control valves.\n");
		return 1;
	}

	pruInit ();

	prussdrv_map_prumem (PRUSS0_PRU1_DATARAM, &pruDataMem);
	pruDataMem_int = (unsigned int*) pruDataMem;

	for (i = 0; i < 6; i++)
                gpioOutputInit (&controlValve[i], 0);

	for (i = 0; i < 12; i++)
		pruDataMem_int[i] = 0;

	if (prussdrv_exec_program (1, "./stepper.bin")) {
		fprintf (stderr, "prussdrv_exec_program (1, './stepper.bin') failed\n");
		return 1;
	}

	if (atoi (argv[3]) == 0)
		for (i = 0; i < 12; i++)
			pruDataMem_int[i] = -atoi (argv[1]);
	else
		for (i = 0; i < 12; i++)
			pruDataMem_int[i] = atoi (argv[1]);

	while (puts (""))
		for (i = 0; i < 12; i++)
			printf ("%d, ", pruDataMem_int[91 + i]);

shutdown:
	if (atoi (argv[3]) == 0)
		for (i = 0; i < 12; i++)
			pruDataMem_int[i] = -atoi (argv[2]);
	else
		for (i = 0; i < 12; i++)
			pruDataMem_int[i] = atoi (argv[2]);

	for (i = 0; i < 12; puts ("")) {
		if ((abs (pruDataMem_int[91 + i]) >> 1) == atoi (argv[2]))
			i++;
		for (j = 0; j < 12; j++)
			printf ("%d, ", pruDataMem_int[91 + j]);
	}

	for (i = 0; i < 6; i++)
                gpioOutputAbscond (&controlValve[i], 0);

	pruAbscond ();

	return 0;
}

