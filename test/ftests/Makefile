CFLAGS=-c -g -O0 -Wall -Wextra -pedantic -std=gnu99 -I. -I../../ebnlib
LDFLAGS=-L. -L../.. -lebnlib -lpthread --coverage
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
CC=gcc

all: $(SOURCES) client server timers run

.c.o:
	$(CC) $(CFLAGS) $< -o $@

client: $(OBJECTS)
	$(CC) -o $@ $@.o $(LDFLAGS)

server: $(OBJECTS)
	$(CC) -o $@ $@.o $(LDFLAGS)

timers: $(OBJECTS)
	$(CC) -o $@ $@.o $(LDFLAGS)

run:
	python ftest.py

clean:
	find . -type f -name "*.py?" -delete
	rm -f *.o client server timers
