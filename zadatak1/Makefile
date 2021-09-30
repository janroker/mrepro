PROJECT = proba
SOURCE = proba.c
HEADERS = proba.h

CC = cc
OBJECTS = ${SOURCE:.c=.o}

$(PROJECT): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(PROJECT)

$(OBJECTS): $(HEADERS) $(SOURCE)
	cc -c $(SOURCE)

clean:
	-rm -f $(PROJECT) $(OBJECTS) *.core
