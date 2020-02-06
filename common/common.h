#ifndef COMMON_H
#define COMMON_H

#define TEENSY1 0
#define TEENSY2 1
#define TEENSY3 2
#define BBB 4

#if BOARD < 0 || BOARD > 4
#error
#endif

#endif

