# Modify the CppUTest paths below if necessary
CPPUTEST_INCDIR = /usr/include/CppUTest
CPPUTEST_LIBDIR = .
CPPFLAGS += \
	-c -Wall -Wextra -pedantic \
	-DPTHREAD -DDEBUG \
	-I. -I../../ebnlib -I../../source \
	-I$(CPPUTEST_INCDIR)
LDFLAGS += \
	-L. -L../.. \
	-L$(CPPUTEST_LIBDIR) \
	-lstdc++ \
	-lCppUTest \
	-lCppUTestExt \
	-lebnlib \
	--coverage
OBJECTS=$(SOURCES:.cpp=.o)
SOURCES=$(wildcard *.cpp)
EXECUTABLE=utest
CC=g++

all: $(SOURCES) $(EXECUTABLE) run

.cpp.o:
	$(CC) $(CPPFLAGS) $< -o $@

$(EXECUTABLE): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

run:
	./$(EXECUTABLE) 2> /dev/null

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
