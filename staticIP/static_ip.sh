#!/bin/sh
ip link set eth0 up
ip addr add `cat /root/.ip_addr/bbb_addr`/24 broadcast 192.168.0.255 dev eth0
ip route add default via 192.168.0.1
