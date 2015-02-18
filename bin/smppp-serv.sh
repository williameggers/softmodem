#!/bin/sh

./sm -dcp $@ &
sleep 1
ppp sm-srv &

