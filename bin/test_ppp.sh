#!/bin/sh

if [ `uname` != "OpenBSD" ]; then
    echo "works on OpenBSD only"
    exit 1
fi

export MALLOC_OPTIONS='S'

rm -f sm1.core sm2.core sm1.log sm2.log
echo "run sm1 as root using sudo"
sudo ./sm -n sm1 -dp $@ &
sleep 1
echo "run sm2 as root using sudo"
sudo ./sm -n sm2 -dcp $@ &

sleep 5
echo "connect them"
sudo -u _sm jack_connect sm1:input sm2:output && sudo -u _sm jack_connect sm1:output sm2:input && echo "."

sleep 5
echo "run ppp server on sm1"
ppp -ddial sm-srv &
sleep 3
echo "run ppp client on sm2"
ppp -ddial sm-cli &
echo "waiting 8 seconds for the ppp connection"
sleep 8
echo "setting the tun0 and tun1 to separated rdomains"
sudo ifconfig tun0 rdomain 1
sudo ifconfig tun0 10.0.0.2 10.0.0.1
sudo ifconfig tun1 rdomain 2
sudo ifconfig tun1 10.0.0.1 10.0.0.2
echo "-> that should work :) don't forget to pkill ppp after usage"

echo "sleeping"
sleep 3600
echo "and so ?"
