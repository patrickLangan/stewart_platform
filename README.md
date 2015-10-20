#Single Cylinder Pneumatic Tests
==============

##Wiring
	spi-0
		Clock:  P9_22
		Data0:  P9_21 (MISO)
	spi-1
		Clock:  P9_31
		Data0:  P9_29 (MISO)

	i2c-1
		Clock:  P9_19
		Data:   P9_20

	i2c-2
		Clock:  P9_17
		Data:   P9_18

	//Note: cannot enable both i2c-1 and spi-0

	Length sensor - PRU0
		Clock:			P8_11
		Sensor1Data:	P8_15
		Sensor2Data:	P8_16

	Length sensor - PRU1
		STEP1:		P8_45
		DIR1:		P8_46
		STEP2:		P8_43
		DIR2:		P8_44
		CtrlExtend:	P8_41
		CtrlRetract:	P8_42

##TODO
- Accelerometer
- Flow sensor
- Temperature sensor
- Stepper valves
- Directional control valve
- Calibration
- Error checking
- General cleanup

##Beaglebone Black setup
###SD card installation
- SD images are here: http://beagleboard.org/latest-images
- Installation instructions are here (Update board with latest software): http://beagleboard.org/getting-started

###Disabling eMMC and HDMI
Disabling eMMC (the 4gb onboard memory) frees up pins P8.20-25 and 3-6, disabling HDMI frees up pins P8.27-46.  To do so:
- mount /dev/mmcblk0p1 /mnt
- vim /mnt/uEnv.txt
- Add the following to uEnv.txt:
```
cape_disable=capemgr.disable_partno=BB-BONELT-HDMI,BB-BONELT-HDMIN,BB-BONE-EMMC-2G
mmcreset=mmc dev 1; mmc rstn 1; gpio set 52
uenvcmd=run mmcreset;
```
- umount /mnt
- poweroff

