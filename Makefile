CFLAGS=-c -g -O0 -Wall -Wextra -pedantic -fPIC -std=gnu99
LDFLAGS=-L. -lebnlib -lpthread
SOURCES=ebnlib.c client.c server.c
OBJECTS=$(SOURCES:.c=.o)
LIBRARY=libebnlib.a
CC=gcc

ifeq ($(COVERAGE),1)
	CFLAGS += --coverage
	LDFLAGS += --coverage
endif

all: $(SOURCES) ebnlib client server

.c.o:
	$(CC) $(CFLAGS) $< -o $@

ebnlib: $(OBJECTS)
	ar rcs $(LIBRARY) ebnlib.o

client: $(OBJECTS)
	$(CC) -o $@ client.o $(LDFLAGS)

server: $(OBJECTS)
	$(CC) -o $@ server.o $(LDFLAGS)

lcov:
	lcov -d . --capture --output-file ebnlib.info
	genhtml -o lcov ebnlib.info

clean:
	rm -f *.a *.o *.gcno *.gcda *.info client server
	rm -rf lcov
