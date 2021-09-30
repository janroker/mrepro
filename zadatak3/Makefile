default: all

CFLAGS = -Wall -Wextra -g
CC = cc $(DEBUG) $(CFLAGS)
LDFLAGS =

all: tcpserver tcpklijent

UTILS_O = debug.o util.o
SERVER_O = $(UTILS_O) tcpserver.o 
KLIJENT_O = tcpklijent.o $(UTILS_O)

tcpserver: $(SERVER_O)
	$(CC) -o $@ $(SERVER_O)
	
tcpklijent: $(KLIJENT_O)
	$(CC) -o $@ $(KLIJENT_O)
	
clean:
	-rm -f tcpserver tcpklijent *.o *.core

debug.o: debug.c debug.h
util.o:	util.c util.h
tcpserver.o: $(UTILS_O) tcpserver.c tcpserver.h
tcpklijent.o: $(UTILS_O) tcpklijent.c
