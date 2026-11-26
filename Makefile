# Weave - build file.
#   make            build the library, tests, apps and benchmarks
#   make test       run T1-T11          make valgrind   memory-check every test/app
#   make bench      run benchmarks      make demo       guided live demo
#   make clean      remove build/
CC      = gcc
CFLAGS  = -std=c11 -D_GNU_SOURCE -Wall -Wextra -g -Iinclude
SRC     = $(wildcard src/*.c)
OBJ     = $(patsubst src/%.c,build/%.o,$(SRC))
TESTS   = $(patsubst tests/%.c,build/%,$(wildcard tests/test_*.c))
PROGS   = build/app_prodcons build/app_mixed build/app_workqueue build/app_bank build/bench_switch build/bench_sync build/bench_scale

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
	$(CC) $(CFLAGS) $< build/libweave.a -o $@ -pthread

test: all
	./tests/run_tests.sh

# Memory check of every test and app (needs valgrind)
valgrind: all
	./tests/run_valgrind.sh

# Benchmarks against pthreads; CSV in bench/results/, graphs in docs/figures/
bench: all
	./bench/run_benchmarks.sh

# Guided live demonstration for the final presentation
demo: all
	./demo.sh

clean:
	rm -rf build

.PHONY: all test valgrind bench demo clean
