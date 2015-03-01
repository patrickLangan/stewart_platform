#!/bin/bash

disable_service(){
	mv /etc/rc2.d/S*"$1" /etc/rc2.d/K01"$1"
	mv /etc/rc3.d/S*"$1" /etc/rc3.d/K01"$1"
	mv /etc/rc4.d/S*"$1" /etc/rc4.d/K01"$1"
	mv /etc/rc5.d/S*"$1" /etc/rc5.d/K01"$1"
	update-rc.d $1 defaults
}

# make sure the script is given an argument
if [ $# -lt 1 ]; then
  echo "Provide the desired static IP address as an argument"
  exit -1
fi

mkdir /root/.ip_addr
echo $1 > /root/.ip_addr/bbb_addr

ln -s `pwd`/static_ip.sh /usr/local/bin/static_ip
ln -s `pwd`/static_ip.service /etc/systemd/system/static_ip.service

disable_service apache2
#disable_service avahi-daemon
#disable_service udhcpd
disable_service wicd

systemctl enable `pwd`/static_ip.service

exit 0
