CC = gcc
CFLAGS = `pkg-config --cflags freetype2` -I../include
LDFLAGS =
LIBS = `pkg-config --libs freetype2`

.PHONY: all clean
.SUFFIXES: .c .o

all: fuzzer font icon-converter icon-converter

fuzzer: fuzzer.o
	$(CC) -g -L../src/ -Wl,-R../src -o $@ fuzzer.o -lMw

font: font.o
	$(CC) $(LDFLAGS) -o $@ font.o $(LIBS)

icon-converter: icon-converter.o
	$(CC) -o $@ icon-converter.o -lm

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o font fuzzer icon-converter
