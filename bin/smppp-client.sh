#!/bin/sh

./sm -dp $@ &
sleep 1
ppp sm-cli &

