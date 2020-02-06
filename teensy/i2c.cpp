#include "i2c.h"

#include "WProgram.h"
#include "pins_arduino.h"
#include "i2c_t3/i2c_t3.h"

const int pin_SCL[4] = {19, 37, 3, 57};
const int pin_SDA[4] = {18, 38, 4, 56};

void i2c_init(int32_t rate, int32_t timeout)
{
	int i;

	for (i = 0; i < 4; i++) {
		pinMode(pin_SCL[i],INPUT);
		pinMode(pin_SDA[i],INPUT);
	}

	Wire.begin(I2C_MASTER, 0x00, I2C_PINS_18_19, I2C_PULLUP_EXT, rate);
	Wire1.begin(I2C_MASTER, 0x00, I2C_PINS_37_38, I2C_PULLUP_EXT, rate);
	Wire2.begin(I2C_MASTER, 0x00, I2C_PINS_3_4, I2C_PULLUP_EXT, rate);
	Wire3.begin(I2C_MASTER, 0x00, I2C_PINS_56_57, I2C_PULLUP_EXT, rate);

	Wire.setDefaultTimeout(timeout);
	Wire1.setDefaultTimeout(timeout);
	Wire2.setDefaultTimeout(timeout);
	Wire3.setDefaultTimeout(timeout);
}

int i2c_read(uint8_t *buff, int channel, int addr, int n)
{
	switch (channel) {
	default:
	case 0:
		Wire.requestFrom(addr, n);
		if (Wire.getError())
			return 1;
		Wire.read(buff, n);
		break;
	case 1:
		Wire1.requestFrom(addr, n);
		if (Wire1.getError())
			return 1;
		Wire1.read(buff, n);
		break;
	case 2:
		Wire2.requestFrom(addr, n);
		if (Wire2.getError())
			return 1;
		Wire2.read(buff, n);
		break;
	case 3:
		Wire3.requestFrom(addr, n);
		if (Wire3.getError())
			return 1;
		Wire3.read(buff, n);
	}

	return 0;
}

