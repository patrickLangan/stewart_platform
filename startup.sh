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

cat /sys/devices/bone_capemgr.*/slots | grep siegx3_cnc > /dev/null

if [ $? -eq 0 ]; then 
	echo "siegx3_cnc  dto already applied"
	exit 1
fi

echo siegx3_cnc > /sys/devices/bone_capemgr.*/slots

if [ $? -eq 1 ]; then 
	echo "siegx3_cnc dto was not accepted."
	exit 1
fi

echo "applied the siegx3_cnc dto"

usleep 200000

enableGpio 30 "out" "0"
enableGpio 60 "out" "0"
enableGpio 31 "out" "0"
