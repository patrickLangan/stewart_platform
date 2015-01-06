pneumaticTests
==============

##BBB setup
Setup PRU based on: http://www.element14.com/community/community/designcenter/single-board-computers/next-gen_beaglebone/blog/2013/05/22/bbb--working-with-the-pru-icssprussv2
and: https://github.com/beagleboard/am335x_pru_package

##Wiring
	Stepper motor
		DIR: P9.11
		PWM: P9.14
	Accelerometer
		SCL: P9.19
		SDA: P9.20
	Linear Capacitive Sensor
		SYS:	P8.11
		DataX:	P8.12
		DataY:	P8.15
		DataZ:	P8.16

##Notes
- Currently using this device tree overlay: github.com/patrickLangan/cncDevTree

##TODO
- Add code for second stepper
- Add code for pressure sensors
- Continuously record sensor readings
- Numericaly integrate acceleration readings
- Replace PWM with PRU for stepper control
- Add accurate timing with the PRU
- Make vectors of arbitrary variable type
- Error checking, and handling
- Use i2c sensor interupts
- Fix variable names
- General cleanup
- Add comments
- Fix PRU code based on new library

