#!/bin/bash

disable_service(){
	mv /etc/rc2.d/S"$1""$2" /etc/rc2.d/K"$1""$2"
	mv /etc/rc3.d/S"$1""$2" /etc/rc3.d/K"$1""$2"
	mv /etc/rc4.d/S"$1""$2" /etc/rc4.d/K"$1""$2"
	mv /etc/rc5.d/S"$1""$2" /etc/rc5.d/K"$1""$2"
	update-rc.d $2 defaults
}

# make sure the script is given an argument
if [ $# -lt 1 ]; then
  echo "Provide the desired static IP address as an argument"
  exit -1
fi

echo $1 > ./bbb_addr

ln -sr ./bbb_addr /root/.ip_addr/bbb_addr
ln -sr ./static_ip.sh /usr/local/bin/static_ip
ln -sr ./static_ip.service /etc/systemd/system/static_ip.service

disable_service 01 apache2
#disable_service 02 avahi-daemon
#disable_service 01 udhcpd
disable_service 01 wicd

systemctl enable `pwd`/static_ip.service

exit 0
