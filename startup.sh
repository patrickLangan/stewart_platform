#!/bin/bash

enableGpio(){
	if [ -f /sys/class/gpio/gpio"$1" ]; then
		echo gpio"$1" already enabled
	else
		echo "$1" > /sys/class/gpio/export
	fi

	if [ -f /sys/class/gpio/gpio"$1"/direction ]; then
		echo "$2" > /sys/class/gpio/gpio"$1"/direction
	else
		echo "$1" gpio direction file not found
		exit 0;
	fi

	if [ -f /sys/class/gpio/gpio"$1"/value ]; then
		if [ "$2" == "out" ]; then
			echo "$3" > /sys/class/gpio/gpio"$1"/value
		fi
	else
		echo "$1" gpio value file not found
		exit 0;
	fi
}

echo stepper-pru > /sys/devices/bone_capemgr.*/slots
echo gpio-enable > /sys/devices/bone_capemgr.*/slots
echo SPI0 > /sys/devices/bone_capemgr.*/slots
echo SPI1 > /sys/devices/bone_capemgr.*/slots
echo cape-bone-iio > /sys/devices/bone_capemgr.*/slots
cat /sys/devices/bone_capemgr.*/slots

sleep 1

#Motor DIR
enableGpio 63 "out" 0
enableGpio 33 "out" 0
enableGpio 34 "out" 0
enableGpio 35 "out" 0
enableGpio 36 "out" 0
enableGpio 37 "out" 0
enableGpio 38 "out" 0
enableGpio 39 "out" 0
enableGpio 48 "out" 0
enableGpio 45 "out" 0
enableGpio 46 "out" 0
enableGpio 47 "out" 0

#Control valve
enableGpio 65 "out" 0
enableGpio 66 "out" 0
enableGpio 67 "out" 0
enableGpio 68 "out" 0
enableGpio 69 "out" 0
enableGpio 12 "out" 0

#Rotary encoder 1
enableGpio 7  "in"
enableGpio 30 "in"
enableGpio 9  "in"
enableGpio 8  "in"
enableGpio 11 "in"
enableGpio 13 "in"
enableGpio 14 "in"
enableGpio 15 "in"
enableGpio 20 "in"
enableGpio 22 "in"
enableGpio 23 "in"
enableGpio 26 "in"

#Rotary encoder 2
enableGpio 44 "in"
enableGpio 49 "in"
enableGpio 50 "in"
enableGpio 51 "in"
enableGpio 60 "in"
enableGpio 61 "in"
enableGpio 32 "in"
enableGpio 78 "in"
enableGpio 79 "in"
enableGpio 80 "in"
enableGpio 81 "in"
enableGpio 89 "in"

#SPI CS
enableGpio 27 "out" 0
enableGpio 10 "out" 0
enableGpio 31 "out" 0

