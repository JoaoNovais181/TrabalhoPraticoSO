#! /bin/bash

for ((i=0 ; i<= $1 ; i++))
do
    bin/sdstore proc-file samples/lusiadas.txt output/lusiadas-proc{$i}.txt nop bcompress& 
done

echo "Acabou Script" &
