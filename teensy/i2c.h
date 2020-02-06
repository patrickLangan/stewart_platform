#ifndef I2C_H
#define I2C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void i2c_init(int32_t rate, int32_t timeout);

int i2c_read(uint8_t *buff, int channel, int addr, int n);

#ifdef __cplusplus
}
#endif

#endif

