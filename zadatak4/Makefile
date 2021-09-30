CFLAGS = -Wall -Wextra -g
CC = cc $(DEBUG) $(CFLAGS)
LDFLAGS =

all: tftpserver

server_OBJECTS = debug.o tftpserver.o util.o # cini se da moraju bit iznad cc bla bla jer se ne prevode kao dependency inace...

tftpserver: $(server_OBJECTS)
	$(CC) -o $@ $(server_OBJECTS)

clean:
	-rm -f tftpserver *.o *.core

debug.o: debug.c debug.h
util.o: debug.o util.c util.h
tftpserver.o: debug.o util.o tftpserver.c
