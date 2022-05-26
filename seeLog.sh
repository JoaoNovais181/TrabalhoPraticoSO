#! /bin/sh

while [ 1 ]
do
	clear;
	ls -I "*.c" -I "*.o" -I "*.txt" -I "*.h" -I "server" -I "client" -I "Makefile" -I "*.sh";
	echo "";
	cat log.txt;
	sleep 1;
done

