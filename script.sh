#! /bin/bash

for ((i=0 ; i<= $1 ; i++))
do
    ./client nop bcompress &
done