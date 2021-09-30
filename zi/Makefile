CFLAGS = -Wall -Wextra -pedantic -g
CC = cc $(DEBUG)
LDFLAGS =

all: sender receiver

SENDER_OBJECTS = debug.o util.o sender.o # cini se da moraju bit iznad cc bla bla jer se ne prevode kao dependency inace...
RECEIVER_OBJECTS = debug.o receiver.o util.o

sender: $(SENDER_OBJECTS)
	$(CC) -o $@ $(SENDER_OBJECTS)

receiver: $(RECEIVER_OBJECTS)
	$(CC) -o $@ $(RECEIVER_OBJECTS)

clean:
	-rm -f sender receiver *.o *.core

debug.o: debug.c debug.h
util.o: debug.o util.c util.h
sender.o: debug.o util.o sender.c
receiver.o: debug.o util.o receiver.c
