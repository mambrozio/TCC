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

hybrid: hybrid.c
	clang++ -o interpret interpret.cpp `llvm-config --cxxflags --ldflags --libs all --system-libs`
	./interpret
	llc interpret.ll
	gcc -c interpret.s
	$(CC) $(CFLAGS) $< interpret.o -o $@
	
clean:
	rm -rf $(GENERATED)

clean-hybrid:
	rm -rf interpret interpret.ll interpret.s interpret.o hybrid
