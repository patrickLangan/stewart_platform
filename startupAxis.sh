#!/bin/bash
echo /sys/devices/ocp.*/"$2"_axis_pwm.*/run
echo /sys/devices/ocp.*/$2_axis_pwm.*/duty
echo /sys/devices/ocp.*/$2_axis_pwm.*/period
echo /sys/class/gpio/gpio$1/value

echo "1" > /sys/devices/ocp.*/$2_axis_pwm.*/run
echo "0" > /sys/devices/ocp.*/$2_axis_pwm.*/duty
echo "1" > /sys/class/gpio/gpio$1/value

