#!/bin/sh

echo "**************************************" 
THETIME=$(date +%H:%M:%S)
THEDATE=$(date +%m-%d-%y)
echo "TIME: ${THEDATE} ${THETIME}" 
gcc   -o3 -std=gnu89 -ffloat-store -o slgtopngmt2  slgtopngmt.c  -lm /usr/local/lib/libpng14.so -lpthread  -g

#./slgtopngmt lg.slg

exit 0
