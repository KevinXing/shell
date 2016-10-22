CC=gcc
.PHONY:clean

all: w4118_sh

w4118_sh: shell.o
	$(CC) -Wall shell.o -o w4118_sh

shell.o:
	$(CC) -Wall -c shell.c

clean:
	rm w4118_sh shell.o

