#!/bin/bash
echo SPI0 > /sys/devices/bone_capemgr.*/slots
echo SPI1 > /sys/devices/bone_capemgr.*/slots
cat /sys/devices/bone_capemgr.*/slots

