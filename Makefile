CFLAGS=-c -g -O0 -Wall -Wextra -pedantic -fPIC -std=gnu99 -DPTHREAD -DDEBUG -I./ebnlib -I./source
SOURCES=$(wildcard ./source/*.c)
OBJECTS=$(SOURCES:.c=.o)
LIBRARY=libebnlib.a
CC=gcc

ifeq ($(COVERAGE),1)
	CFLAGS += --coverage
endif

all: $(SOURCES) ebnlib tests

.c.o:
	$(CC) $(CFLAGS) $< -o $@

ebnlib: $(OBJECTS)
	ar rcs $(LIBRARY) $(OBJECTS)

tests:
	$(MAKE) -C test/utests all
	$(MAKE) -C test/ftests all

lcov:
	lcov -d ./source --capture --output-file ebnlib.info
	genhtml -o lcov ebnlib.info

clean:
	$(MAKE) -C test/utests clean
	$(MAKE) -C test/ftests clean
	rm -f *.a $(OBJECTS) source/*.gcno source/*.gcda *.info
	if test -d lcov; then rm -rf lcov; fi
