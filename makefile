CC:=gcc
CFLAGS:=--std=c11 --pedantic -Wall -Wextra -lm -O3

LLC:=llc
LLCFLAGS:=-O3 -filetype=obj

SOURCES := $(wildcard examples/*.lua)
BYTECODES := $(patsubst %.lua,%.byte,$(SOURCES))

GENERATED := $(BYTECODES) c-minilua

.PHONY: all clean

all: $(GENERATED)

hybrid-all: hybrid-base hybrid-base-prefetch

examples/%.byte: examples/%.lua
	luac -o $@ $<

c-minilua: c-minilua.c
	$(CC) $(CFLAGS) $< -o $@

hybrid-base: hybrid.c interpret.cpp step-base.cpp
	clang++ -o interpret interpret.cpp `llvm-config --cxxflags --ldflags --libs all --system-libs`
	./interpret
	clang++ -o step-base step-base.cpp `llvm-config --cxxflags --ldflags --libs all --system-libs`
	./step-base
	$(LLC) $(LLCFLAGS) interpret.ll
	$(LLC) $(LLCFLAGS) step-base.ll
	$(CC) $(CFLAGS) $< interpret.o step-base.o -o hybrid-base

hybrid-base-prefetch: hybrid.c interpret.cpp step-base-prefetch.cpp
	clang++ -o interpret interpret.cpp `llvm-config --cxxflags --ldflags --libs all --system-libs`
	./interpret
	clang++ -o step-base-prefetch step-base-prefetch.cpp `llvm-config --cxxflags --ldflags --libs all --system-libs`
	./step-base-prefetch
	$(LLC) $(LLCFLAGS) interpret.ll
	$(LLC) $(LLCFLAGS) step-base-prefetch.ll
	$(CC) $(CFLAGS) $< interpret.o step-base-prefetch.o -o hybrid-base-prefetch

hybrid-indirect: hybrid.c interpret.cpp step-indirect.cpp
	clang++ -o interpret interpret.cpp `llvm-config --cxxflags --ldflags --libs all --system-libs`
	./interpret
	clang++ -o step-indirect step-indirect.cpp `llvm-config --cxxflags --ldflags --libs all --system-libs`
	./step-indirect
	$(LLC) $(LLCFLAGS) interpret.ll
	$(LLC) $(LLCFLAGS) step-indirect.ll
	$(CC) $(CFLAGS) $< interpret.o step-indirect.o -o hybrid-indirect

clean:
	rm -rf $(GENERATED)

clean-hybrid:
	rm -rf interpret step-base step-base-prefetch step-indirect interpret.ll step-base.ll step-base-prefetch.ll step-indirect.ll interpret.s step-base.s step-base-prefetch.s step-indirect.s interpret.o step-base.o step-base-prefetch.o step-indirect.o hybrid-base hybrid-base-prefetch hybrid-indirect
