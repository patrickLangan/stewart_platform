#include "util.h"
#include <limits.h>

/* Compute time delta, accounting for rollover in t2 */
uint32_t time_delta_us(uint32_t t2, uint32_t t1)
{
	if (t2 > t1)
		return t2 - t1;
	else
		return t2 + (UINT_MAX - t1);
}

int limit_frequency_us(uint32_t time, uint32_t *last_time, uint32_t period)
{
	uint32_t dt;

	dt = time_delta_us(time, *last_time);
	if (dt < period)
		return 1;
	*last_time = time;

	return 0;
}

int ilimit(int val, int min, int max)
{
	if (val > max)
		val = max;
	else if (val < min)
		val = min;

	return val;
}

float flimit(float val, float min, float max)
{
	if (val > max)
		val = max;
	else if (val < min)
		val = min;

	return val;
}

