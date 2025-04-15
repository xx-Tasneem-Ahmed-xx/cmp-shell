# Compiler
CC = gcc

all: cmpsh

cmpsh: cmpsh.o
	$(CC) cmpsh.o -o cmpsh

cmpsh.o: cmpsh.c
	$(CC) -c cmpsh.c

clean:
	rm -f *.o cmpsh