CFLAGS = -Wall -Wextra -g
CC = cc $(DEBUG) $(CFLAGS)
LDFLAGS =

all: bot server

BOT_OBJECTS = debug.o util.o bot.o # cini se da moraju bit iznad cc bla bla jer se ne prevode kao dependency inace...
server_OBJECTS = debug.o server.o util.o

bot: $(BOT_OBJECTS)
	$(CC) -o $@ $(BOT_OBJECTS)

server: $(server_OBJECTS)
	$(CC) -o $@ $(server_OBJECTS)

clean:
	-rm -f bot server *.o *.core

debug.o: debug.c debug.h
util.o: debug.o util.c util.h
bot.o: debug.o util.o bot.c
server.o: debug.o util.o server.c
