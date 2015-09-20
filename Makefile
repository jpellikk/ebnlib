CFLAGS=-c -g -O0 -Wall -Wextra -pedantic -fPIC -std=gnu99
LDFLAGS=-lpthread
SOURCES=network.c client.c server.c
OBJECTS=$(SOURCES:.c=.o)
CC=gcc

ifeq ($(COVERAGE),1)
	CFLAGS += --coverage
	LDFLAGS += --coverage
endif

all: $(SOURCES) network client server

.c.o:
	$(CC) $(CFLAGS) $< -o $@

network: $(OBJECTS)
	ar rcs $@.a network.o

client: $(OBJECTS)
	$(CC) -o $@ client.o network.a $(LDFLAGS)

server: $(OBJECTS)
	$(CC) -o $@ server.o network.a $(LDFLAGS)

lcov:
	lcov -d . --capture --output-file network.info
	genhtml -o lcov network.info

clean:
	rm -f *.a *.o *.gcno *.gcda *.info client server
	rm -rf lcov
