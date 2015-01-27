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
echo i2c1-400hz > /sys/devices/bone_capemgr.*/slots
echo i2c2-400hz > /sys/devices/bone_capemgr.*/slots
echo cape-bone-iio > /sys/devices/bone_capemgr.*/slots
cat /sys/devices/bone_capemgr.*/slots

sleep 1

enableGpio 38 "out" 0
enableGpio 39 "out" 0
enableGpio 34 "out" 0
enableGpio 35 "out" 0
enableGpio 45 "out" 0
enableGpio 44 "out" 0
enableGpio 47 "out" 0
enableGpio 46 "out" 0
enableGpio 37 "out" 0
enableGpio 36 "out" 0
enableGpio 33 "out" 0
enableGpio 32 "out" 0
enableGpio 61 "out" 0

enableGpio 65  "out" 0
enableGpio 89  "out" 0
enableGpio 81  "out" 0
enableGpio 80  "out" 0
enableGpio 78  "out" 0
enableGpio 79  "out" 0

cat /sys/kernel/debug/gpio

