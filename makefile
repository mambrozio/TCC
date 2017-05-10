CC:=gcc
CFLAGS:=--std=c11 --pedantic -Wall -Wextra -lm -O3

SOURCES := $(wildcard examples/*.lua)
BYTECODES := $(patsubst %.lua,%.byte,$(SOURCES))

GENERATED := $(BYTECODES) c-minilua

.PHONY: all clean

all: $(GENERATED)

examples/%.byte: examples/%.lua
	luac -o $@ $<

c-minilua: c-minilua.c
	$(CC) $(CFLAGS) $< -o $@
	
clean:
	rm -rf $(GENERATED)
