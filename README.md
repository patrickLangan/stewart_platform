#Single Cylinder Pneumatic Tests
==============

##TODO
- Data transfer between bbb's
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

