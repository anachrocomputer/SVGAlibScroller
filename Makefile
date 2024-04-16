
CC=gcc
LD=gcc

CFLAGS=-O3 -mtune=native

all: scroller

scroller.o: scroller.c
	$(CC) $(CFLAGS) -c scroller.c

scroller: scroller.o
	$(LD) -o scroller scroller.o -lm -lvgagl -lvga -lasound
	
