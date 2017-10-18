CC:=gcc
CFLAGS:=--std=c11 --pedantic -Wall -Wextra -lm -O3

LLC:=llc
LLCFLAGS:=-O3 -filetype=obj

SOURCES := $(wildcard examples/*.lua)
BYTECODES := $(patsubst %.lua,%.byte,$(SOURCES))

GENERATED := $(BYTECODES) c-minilua

.PHONY: all clean

all: $(GENERATED)

examples/%.byte: examples/%.lua
	luac -o $@ $<

c-minilua: c-minilua.c
	$(CC) $(CFLAGS) $< -o $@

hybrid: hybrid.c interpret.cpp step.cpp
	clang++ -o interpret interpret.cpp `llvm-config --cxxflags --ldflags --libs all --system-libs`
	./interpret
	clang++ -o step step.cpp `llvm-config --cxxflags --ldflags --libs all --system-libs`
	./step
	$(LLC) $(LLCFLAGS) interpret.ll
	$(LLC) $(LLCFLAGS) step.ll
	$(CC) $(CFLAGS) $< interpret.o step.o -o $@

clean:
	rm -rf $(GENERATED)

clean-hybrid:
	rm -rf interpret step interpret.ll step.ll interpret.s step.s interpret.o step.o hybrid
