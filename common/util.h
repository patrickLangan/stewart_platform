#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

uint32_t time_delta(uint32_t t1, uint32_t t2);

int limit_frequency(uint32_t time, uint32_t *last_time, uint32_t period);

int ilimit(int val, int min, int max);

float flimit(float val, float min, float max);

#endif

