#!/bin/bash
echo length-pru > /sys/devices/bone_capemgr.*/slots
echo i2c1-400hz > /sys/devices/bone_capemgr.*/slots
echo i2c2-400hz > /sys/devices/bone_capemgr.*/slots
cat /sys/devices/bone_capemgr.*/slots

