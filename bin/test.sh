#!/bin/sh

export MALLOC_OPTIONS='S'

rm -f sm.core sm1.log sm2.log sm1.tty sm2.tty
echo "run sm1"
./sm -c -g -n sm1 $@ &
echo "run sm2"
./sm -n sm2 $@ &

sleep 1
echo "sm1:`cat sm1.tty` sm2:`cat sm2.tty`"
sleep 6
echo -n "connect them"
jack_connect sm1:input sm2:output && jack_connect sm1:output sm2:input && echo "."

echo "sleeping"
sleep 3600
echo "and so ?"
