#Simpliest Makefile
#MichealYang @ 2015/10/20

CC=gcc

COMMON_FLAGS=-Wno-write-strings

server: nweb.o
	$(CC) -o server nweb.o
nweb.o: nweb.cpp
	$(CC) -c $(COMMON_FLAGS) nweb.cpp
