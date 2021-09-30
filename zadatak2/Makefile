PROJECT = prog
SOURCE = main.c

#CC = gcc ... ako zelimo kompajler gcc; default je cc
CFLAGS = -Wall -g
LDFLAGS =

OBJECTS = ${SOURCE:.c=.o}

$(PROJECT): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(PROJECT)

$(OBJECTS): $(HEADERS) $(SOURCE)

clean:
	-rm -f $(PROJECT) $(OBJECTS) *.core
