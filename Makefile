# Weave - build file.  make = build all, make test = run tests, make clean = remove build/
CC      = gcc
CFLAGS  = -std=c11 -D_GNU_SOURCE -Wall -Wextra -g -Iinclude
SRC     = $(wildcard src/*.c)
OBJ     = $(patsubst src/%.c,build/%.o,$(SRC))
TESTS   = $(patsubst tests/%.c,build/%,$(wildcard tests/test_*.c))
PROGS   = build/app_prodcons build/app_mixed build/bench_switch

all: build/libweave.a $(TESTS) $(PROGS)

build:
	mkdir -p build

build/%.o: src/%.c src/internal.h include/weave.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/libweave.a: $(OBJ)
	ar rcs $@ $^

build/test_%: tests/test_%.c build/libweave.a | build
	$(CC) $(CFLAGS) $< build/libweave.a -o $@

build/app_%: apps/app_%.c build/libweave.a | build
	$(CC) $(CFLAGS) $< build/libweave.a -o $@

build/bench_%: bench/bench_%.c build/libweave.a | build
	$(CC) $(CFLAGS) $< build/libweave.a -o $@

test: all
	./tests/run_tests.sh

clean:
	rm -rf build

.PHONY: all test clean
