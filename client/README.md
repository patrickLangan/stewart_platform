#Client - Single Cylinder Control
==============

##Wiring
	i2c-1
		Clock:  P9_19
		Data:   P9_20

	i2c-2
		Clock:  P9_17
		Data:   P9_18

	Length sensor - PRU0
		Clock:			P8_11
		Slave Select:		P8_12
		SensorData:	P8_15

	Stepper valves - PRU1
		STEP1:			P8_46
		STEP2:			P8_45
		DIR1:			P8_44
		DIR2:			P8_43
		CtrlExtend:		P8_41
		CtrlRetract:	P8_42

##TODO
- Correct ADC interface timing
- Error checking
- General cleanup

