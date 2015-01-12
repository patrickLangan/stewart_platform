pneumaticTests
==============

##Beaglebone Black setup
###Disabling HDMI
Disabling HDMI frees up pins P8.27-46.  To do so:
- mount /dev/mmcblk0p1  /mnt
- vim /mnt/uEnv.txt
- Uncomment or add: optargs=capemgr.disable_partno=BB-BONELT-HDMI,BB-BONELT-HDMIN
- umount /mnt
- sudo poweroff
###Disabling eMMC
Disabling eMMC (the 4gb onboard memory) frees up pins P8.20-25 and 3-6.  Before you disable it you need to stop booting from it and switch to an SD card.
- SD images are here: http://beagleboard.org/latest-images
- Instalation instructions are here (Update board with latest software): http://beagleboard.org/getting-started
After booting into the SD card you can make changes similar to how the HDMI was disabled
- mount /dev/mmcblk0p1  /mnt
- vim /mnt/uEnv.txt
- Uncomment or add: cape_disable=capemgr.disable_partno=BB-BONE-EMMC-2G
- Uncomment or add: mmcreset=mmc dev 1; mmc rstn 1; gpio set 52
- Uncomment or add: uenvcmd=run mmcreset;
- umount /mnt
- sudo poweroff
###Setting up and running the control program
- git clone https://github.com/patrickLangan/pneumaticTests.git
- cd ~/pneumaticTests/i2c-400hz
- make
- cd ~/pneumaticTests/stepper-pru
- make
- cd ~/pneumaticTests
- make
- ~/pneumaticTests/startup.sh
- ~/pneumaticTests/control input.file

##Wiring
	i2c-1
		Clock:	P9.19
		Data:	P9.20
	i2c-2
		Clock:	P9.17
		Data:	P9.18
	PRU1
		PinOut:	P8.20-21, 27-29, and 39-46

##TODO
- Error checking, and handling
- Add comments
- Enable 18 GPIO pins (12 stepper driver direction inputs, 6 directional control valve solenoids)
- Add PRU back, this time for stepper control and timming

