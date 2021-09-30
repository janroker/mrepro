CFLAGS = -Wall -Wextra -g
CC = cc $(DEBUG) $(CFLAGS)
LDFLAGS =

ZAMISLI_O = debug.o util.o zamisli.o


zamisli: $(ZAMISLI_O)
	$(CC) -o $@ $(ZAMISLI_O)

clean:
	-rm -f zamisli *.o *.core

debug.o: debug.c debug.h
util.o: debug.o util.c util.h
zamisli.o: debug.o util.o zamisli.c
