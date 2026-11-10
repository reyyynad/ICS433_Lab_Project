# Weave - User-Level Thread Library with Scheduling and Synchronization

Operating Systems Lab Project - Group #1, Section F62 (Phase 2: core implementation, written in C).

| Member | Module | Files |
|---|---|---|
| Renad Adel | Thread lifecycle and context switching | `src/thread.c`, `src/context.c`, tests T1-T2 |
| Nada Alrobaian | Scheduler and preemption | `src/scheduler.c`, `src/timer.c`, tests T3-T4 |
| Shatha Alharbi | Synchronization primitives | `src/mutex.c`, `src/semaphore.c`, `src/waitq.c`, tests T5, T6, T8 |
| Bana Jaber | Applications, testing and evaluation | `src/trace.c`, `apps/`, `bench/`, tests T7, T9 |

## Build and run
```
make          # build the library, tests, apps and benchmark
make test     # run T1-T9 (PASS / FAIL / TODO for each)
make clean
```
Requires gcc, make and Linux x86-64 (ucontext). Use `valgrind --leak-check=full ./build/test_t1_lifecycle` to check for leaks.

## Folder structure
```
include/weave.h     public API (agreed by all members)
src/                library source, one file per module
tests/              test cases T1-T9 and run_tests.sh
apps/               example applications
bench/              benchmarks and plotting script
docs/screenshots/   figures for the Phase 2 report
```

## Comment style (same as the Phase 1 proposal: formal, clear, structured)
- Every file starts with a header block: Module, Owner, Purpose, OS concepts.
- Every function has a comment: Purpose, Params, Returns.
- Write full sentences; mark unfinished work as `TODO (Name): ...`.

## Git workflow
- One branch per member (`renad-thread`, `nada-scheduler`, `shatha-sync`, `bana-tests`), merge into `main` by pull request reviewed by another member.
- Commit often with a clear message (e.g. `Add wv_create and stack allocation`). Each member's commits should be visible in the history.
