CFLAGS = -Wall -Wextra -g
CC = cc $(DEBUG) $(CFLAGS)
LDFLAGS =

all: bot UDP_server

BOT_OBJECTS = debug.o util.o bot.o # cini se da moraju bit iznad cc bla bla jer se ne prevode kao dependency inace...
UDP_server_OBJECTS = debug.o UDP_server.o util.o

bot: $(BOT_OBJECTS)
	$(CC) -o $@ $(BOT_OBJECTS)

UDP_server: $(UDP_server_OBJECTS)
	$(CC) -o $@ $(UDP_server_OBJECTS)

clean:
	-rm -f bot UDP_server *.o *.core

debug.o: debug.c debug.h
util.o: util.c util.h
bot.o: debug.o util.o bot.c
UDP_server.o: debug.o UDP_server.c util.o
