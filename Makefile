
SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

CFLAGS := -g -O2 -Wall -std=gnu11

all : bin/sdstore bin/sdstored

bin/sdstore: obj/sdstore.o obj/Pedido.o obj/readln.o
	gcc $(CFLAGS) obj/sdstore.o obj/Pedido.o obj/readln.o -o bin/sdstore

obj/sdstore.o: src/sdstore.c
	gcc -c -r $(CFLAGS) src/sdstore.c -o obj/sdstore.o

bin/sdstored: obj/sdstored.o obj/readln.o obj/Pedido.o obj/Execute.o
	gcc $(CFLAGS) obj/sdstored.o obj/readln.o obj/Pedido.o obj/Execute.o -o bin/sdstored

obj/sdstored.o: src/sdstored.c 
	gcc -c -r $(CFLAGS) src/sdstored.c -o $@

obj/readln.o : src/readln.c
	gcc -c -r $(CFLAGS) src/readln.c -o $@

obj/Pedido.o : src/Pedido.c
	gcc -c -r $(CFLAGS) src/Pedido.c -o $@

obj/Execute.o : src/Execute.c
	gcc -c -r $(CFLAGS) src/Execute.c -o $@

transformations: nop bcompress bdecompress gcompress gdecompress encrypt decrypt

nop: src/Transformations/nop.c
	gcc src/Transformations/nop.c -o bin/transformations/$@

bcompress: src/Transformations/nop.c 
	gcc src/Transformations/bcompress.c -o bin/transformations/$@

bdecompress: src/Transformations/bdecompress.c
	gcc src/Transformations/bdecompress.c -o bin/transformations/$@

gcompress: src/Transformations/gcompress.c
	gcc src/Transformations/gcompress.c -o bin/transformations/$@

gdecompress: src/Transformations/gdecompress.c
	gcc src/Transformations/gdecompress.c -o bin/transformations/$@

encrypt: src/Transformations/encrypt.c
	gcc src/Transformations/encrypt.c -o bin/transformations/$@

decrypt: src/Transformations/decrypt.c
	gcc src/Transformations/decrypt.c -o bin/transformations/$@

clean:
	rm -rf obj/*.o bin/sdstored bin/sdstore clientToServer

clean-Transformations:
	rm -rf bin/transformations/*
