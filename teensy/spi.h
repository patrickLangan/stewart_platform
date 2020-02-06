#ifndef SPI_H
#define SPI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void spi_init(void);

int spi_read(uint8_t *buff, int channel, int n);

#ifdef __cplusplus
}
#endif

#endif

