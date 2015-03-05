pneumaticTests
==============

##TODO
- Error checking, and handling
- Add comments
- Add in optical length sensors
- Add in rotary encoders
- PID control
- Load motion data from file


##Wiring
	spi-0
		Clock:	P9_22
		Data1:	P9_18 (MOSI)
		Data0:	P9_21 (MISO)

	spi-1
		Clock:	P9_31
		Data1:	P9_30 (MOSI)
		Data0:	P9_29 (MISO)

	SPI CS - GPIO0
		P8_17   gpio0_27        gpio27  0x02c
		P9_11   gpio0_30        gpio30  0x070
		P9_13   gpio0_31        gpio31  0x074

	Motor STEP - PRU1
		P8_45	pru_0
		P8_46	pru_1
		P8_43	pru_2
		P8_44	pru_3
		P8_41	pru_4
		P8_42	pru_5
		P8_39	pru_6
		P8_40	pru_7
		P8_27	pru_8
		P8_29	pru_9
		P8_28	pru_10
		P8_21	pru_12

	Motor DIR - GPIO1
		P8_25	gpio1_0		gpio32	0x000
		P8_24	gpio1_1		gpio33	0x004
		P8_5	gpio1_2		gpio34	0x008
		P8_6	gpio1_3		gpio35	0x00c
		P8_23	gpio1_4		gpio36	0x010
		P8_22	gpio1_5		gpio37	0x014
		P8_3	gpio1_6		gpio38	0x018
		P8_4	gpio1_7		gpio39	0x01c
		P8_12	gpio1_12	gpio44	0x030
		P8_11	gpio1_13	gpio45	0x034
		P8_16	gpio1_14	gpio46	0x038
		P8_15	gpio1_15	gpio47	0x03c

	Control Valve - GPIO2
		P8_18	gpio2_1		gpio65	0x08c
		P8_7	gpio2_2		gpio66	0x090
		P8_8	gpio2_3		gpio67	0x094
		P8_10	gpio2_4		gpio68	0x098
		P8_9	gpio2_5		gpio69	0x09c
		P9_20	gpio0_12	gpio12	0x178

	Rotary Encoder 1 (channel 1) - GPIO0
		P9_42   gpio0_7		gpio7   0x164
		P8_31   gpio0_10	gpio10  0x0d8
		P8_33   gpio0_9		gpio9   0x0d4
		P8_35   gpio0_8		gpio8   0x0d0
		P8_32   gpio0_11	gpio11  0x0dc
		P9_19   gpio0_13	gpio13  0x17c
		P9_26   gpio0_14	gpio14  0x180
		P9_24   gpio0_15	gpio15  0x184
		P9_41   gpio0_20	gpio20  0x1b4
		P8_19   gpio0_22	gpio22  0x020
		P8_13   gpio0_23	gpio23  0x024
		P8_14   gpio0_26	gpio26  0x028

	Rotary Encoder 2 (channel 2) - GPIO0, GPIO1, and GPIO2
		P9_15   gpio1_16	gpio48  0x040
		P9_23   gpio1_17	gpio49  0x044
		P9_14   gpio1_18	gpio50  0x048
		P9_16   gpio1_19	gpio51  0x04c
		P9_12   gpio1_28	gpio60  0x078
		P8_26   gpio1_29	gpio61  0x07c
		P8_20   gpio1_31	gpio63  0x084

		P8_37   gpio2_12	gpio78  0x0c0
		P8_38   gpio2_15	gpio79  0x0c4
		P8_36   gpio2_16	gpio80  0x0c8
		P8_34   gpio2_17	gpio81  0x0cc
		P8_30   gpio2_25	gpio89  0x0ec


##Beaglebone Black setup
###Disabling HDMI
Disabling HDMI frees up pins P8.27-46.  To do so:
- mount /dev/mmcblk0p1  /mnt
- vim /mnt/uEnv.txt
- Uncomment or add: optargs=capemgr.disable_partno=BB-BONELT-HDMI,BB-BONELT-HDMIN
- umount /mnt
- sudo poweroff

###Disabling eMMC
Disabling eMMC (the 4gb onboard memory) frees up pins P8.20-25 and 3-6.  Before disabling it, you need to stop booting from it and switch to an SD card.
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

###Disabling i2c2
Disabling i2c2 frees up pins P9.19-20.  To do so:
- git clone https://github.com/derekmolloy/boneDeviceTree.git
- cd boneDeviceTree/DTSource3.8.13/
- vim am335x-bone-common.dtsi
- Uncomment line 404: pinctrl-0 = <&i2c2_pins>;
- dtc -O dtb -o am335x-boneblack.dtb -b 0 -@ am335x-boneblack.dts
- sudo mv /boot/uboot/dtbs/am335x-boneblack.dtb /boot/uboot/dtbs/am335x-boneblack.orig.dtb
- sudo mv am335x-boneblack.dtb /boot/uboot/dtbs/
- sudo poweroff

instructions from http://www.embedded-things.com/bbb/enable-canbus-on-the-beaglebone-black/

###Setting up and running the control program
- git clone https://github.com/patrickLangan/pneumaticTests.git
- Run all the make files
- ./startup.sh
- ./control

