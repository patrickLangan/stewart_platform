#include "spi.h"

#include "SPI.h"
#include "WProgram.h"
#include "pins_arduino.h"

const int pin_CS[2] = {10, 31};

SPISettings spi_settings(1500000, MSBFIRST, SPI_MODE1);

void spi_init(void)
{
	int i;

	for (i = 0; i < 2; i++) {
		pinMode(pin_CS[i], OUTPUT);
		digitalWriteFast(pin_CS[i], 0);
	}

	SPI.begin();
	SPI1.begin();
}

int spi_read(uint8_t *buff, int channel, int n)
{
	switch (channel) {
	default:
	case 0:
		SPI.beginTransaction(spi_settings);
		digitalWriteFast(pin_CS[channel], 0);
		buff[0] = SPI.transfer(0);
		buff[1] = SPI.transfer(0);
		digitalWriteFast(pin_CS[channel], 1);
		SPI.endTransaction();
		break;
	case 1:
		SPI1.beginTransaction(spi_settings);
		digitalWriteFast(pin_CS[channel], 0);
		buff[0] = SPI1.transfer(0);
		buff[1] = SPI1.transfer(0);
		digitalWriteFast(pin_CS[channel], 1);
		SPI1.endTransaction();
	}

	return 0;
}

