# Weave – User-Level Thread Library with Scheduling and Synchronization

ICS 433 Operating Systems Lab Project · Group #1 · Section F62 · Term 261 (final submission)

Weave is a user-level threads library written in C for Linux x86-64. Thread
creation, context switching, scheduling, preemption and blocking all happen in
user space, inside the program, so every step can be traced and measured. It
uses the **many-to-one** model: all Weave threads run on one kernel thread.

| Member | KFUPM ID | Module | Main files |
|---|---|---|---|
| Renad Adel | 202276760 | Thread lifecycle and context switching | `src/thread.c`, `src/context.c`, T1, T2 |
| Nada Alrobaian | 202263260 | Scheduler and preemption | `src/scheduler.c`, `src/timer.c`, T3, T4 |
| Shatha Alharbi | 202283660 | Synchronization primitives | `src/mutex.c`, `src/semaphore.c`, `src/condvar.c`, `src/waitq.c`, T5, T6, T8, T10, T11 |
| Bana Jaber | 202260960 | Applications, testing and evaluation | `src/trace.c`, `apps/`, `bench/`, T7, T9 |

## Features

- Threads: `wv_create`, `wv_yield`, `wv_exit`, `wv_join` (with return value), `wv_self`; a private 64 KiB stack per thread; up to 1,024 threads.
- Context switching with `getcontext` / `makecontext` / `swapcontext`.
- Two scheduling policies: preemptive **Round Robin** and **Priority with aging**.
- Timer preemption with `setitimer(ITIMER_VIRTUAL)` and `SIGVTALRM`; the signal is masked during every library critical section.
- Synchronization: blocking **mutex** with owner checks, counting **semaphore**, **condition variables** (wait / signal / broadcast, Mesa semantics), plus `trylock`, `trywait` and `destroy`. All waiters are woken in FIFO order.
- Tracing (µs timeline of every state change) and per-thread statistics: run time, waiting time, response time, switch count.
- 11 automated tests, 4 example applications, 3 benchmark programs (pthreads comparison, scalability, time quantum, policy comparison).

## Requirements

- Linux x86-64 (tested on Ubuntu 24.04, gcc 13)
- `gcc` and `make` (`sudo apt install build-essential`)
- Optional: `valgrind` for memory checks, `python3` + `matplotlib` for the graphs (`pip install matplotlib`)

No other libraries are needed. The benchmarks link with `-pthread` to compare against pthreads.

## Build and run

```sh
make              # builds build/libweave.a, all tests, apps and benchmarks
make test         # runs T1-T11, prints PASS / FAIL for each
make valgrind     # runs every test and app under Valgrind (0 errors, 0 leaks expected)
make bench        # runs the benchmarks, writes bench/results/*.csv and docs/figures/*.png
make demo         # guided live demo, one step per Enter key (./demo.sh --no-pause to run all)
make clean
```

Single programs can be run directly, for example:

```sh
./build/test_t5_race --no-lock      # race condition without the mutex
./build/test_t5_race --lock         # same program with the mutex
./build/app_mixed --stats           # Round Robin; add --priority for the Priority policy
./build/app_workqueue --stats       # worker pool (mutex + condition variables + semaphore)
./build/app_bank --no-lock          # bank transfers: money is created or lost without locking
./build/test_t3_preempt --trace     # prints every scheduling event with a timestamp
```

## Using the library in your own program

```c
#include "weave.h"

static wv_mutex_t m;
static long counter;

static void *work(void *arg) {
    for (int i = 0; i < 1000; i++) { wv_mutex_lock(&m); counter++; wv_mutex_unlock(&m); }
    return arg;
}

int main(void) {
    wv_init(WV_POLICY_RR, 10);            /* Round Robin, 10 ms quantum */
    wv_mutex_init(&m);
    wv_tid_t a = wv_create(work, NULL, 1), b = wv_create(work, NULL, 1);
    wv_join(a, NULL); wv_join(b, NULL);
    return counter == 2000 ? 0 : 1;
}
```

Compile: `gcc -Iinclude my_prog.c build/libweave.a -o my_prog`

## API summary (`include/weave.h`)

| Function | Returns / notes |
|---|---|
| `int wv_init(wv_policy_t policy, int quantum_ms)` | Call once in `main`. `WV_POLICY_RR`, `WV_POLICY_PRIORITY`, or `WV_POLICY_PRIORITY_NOAGING` (starvation demo only). |
| `wv_tid_t wv_create(wv_func_t f, void *arg, int priority)` | New thread ID, or `WV_ERR_INVALID` / `WV_ERR_NOMEM` / `WV_ERR_LIMIT`. Higher number = higher priority. |
| `void wv_yield(void)` | Give up the CPU (RUNNING → READY). |
| `void wv_exit(void *retval)` | End the calling thread. The main thread returns from `main` instead. |
| `int wv_join(wv_tid_t tid, void **retval)` | Wait for `tid` to end and collect its result. |
| `wv_tid_t wv_self(void)` | ID of the caller (main thread = 0). |
| `wv_mutex_init / lock / unlock / trylock / destroy` | `unlock` by a non-owner → `WV_ERR_NOTOWNER`; relock by the owner → `WV_ERR_INVALID`; `trylock` on a taken mutex → `WV_ERR_BUSY`. |
| `wv_sem_init / wait / post / trywait / destroy` | Counting semaphore; `trywait` at 0 → `WV_ERR_BUSY`. |
| `wv_cond_init / wait / signal / broadcast / destroy` | `wait` must be called with the mutex held (else `WV_ERR_NOTOWNER`). Re-check the condition in a `while` loop. |
| `void wv_trace_enable(int on)` / `void wv_stats_print(void)` | Event timeline / per-thread statistics table. |

Error codes: `WV_OK 0`, `WV_ERR_NOTIMPL -1`, `WV_ERR_INVALID -2`, `WV_ERR_NOMEM -3`, `WV_ERR_LIMIT -4`, `WV_ERR_NOTOWNER -5`, `WV_ERR_BUSY -6`. `destroy` returns `WV_ERR_BUSY` while threads still wait on the object.

## Tests

| Test | What it checks |
|---|---|
| T1 `test_t1_lifecycle` | 10 threads created, run and joined; return values; no leaks |
| T2 `test_t2_yield` | strict A B A B order with `wv_yield` |
| T3 `test_t3_preempt` | a thread in an infinite loop is preempted by the timer |
| T4 `test_t4_priority` | without aging a priority-1 thread starves behind three priority-10 threads; with aging it runs |
| T5 `test_t5_race` | 4 × 1,000,000 increments: updates lost without the mutex, exactly 4,000,000 with it |
| T6 `test_t6_semaphore` | bounded buffer (size 5, 3 producers, 3 consumers): 60 items, each consumed once |
| T7 `test_t7_blocking` | `sleep(2)` in one thread stalls all threads (many-to-one limitation) |
| T8 `test_t8_errors` | 16 wrong calls return their documented error codes; library keeps working |
| T9 `test_t9_stress` | 500 threads, one mutex, 1 ms quantum: no lost update, no deadlock |
| T10 `test_t10_condvar` | monitor bounded buffer (400 items), signal wakes one in FIFO order, broadcast wakes all, trylock/trywait/destroy errors (21 checks) |
| T11 `test_t11_sync_policies` | mutex and semaphore with timer-only preemption under both RR and Priority |

`tests/run_tests.sh` uses the exit code of each program: 0 = PASS, 1 = FAIL, 2 = not implemented.

## Benchmarks

`make bench` runs `bench/run_benchmarks.sh`, which writes:

- `bench/results/switch.csv` – thread create + join and context switch, Weave vs pthreads
- `bench/results/sync.csv` – mutex (uncontended and contended), semaphore and condvar hand-off, Weave vs pthreads
- `bench/results/scale_threads.csv` – switch cost for 2–1,000 ready threads, RR vs Priority
- `bench/results/quantum.csv` – time quantum 1–100 ms: response time and number of switches
- `bench/results/policies.csv` – 4 jobs under RR, Priority + aging, Priority without aging: response, turnaround and waiting time
- `bench/results/environment.txt` – machine, kernel and compiler used
- `docs/figures/*.png` – the graphs used in the report (drawn by `bench/plot.py`)

Every value is the best of several repetitions (5 for `bench_switch`/`bench_sync`, 3 for `bench_scale`). Numbers depend on the machine; rerun on yours to compare.

## Folder structure

```
include/weave.h          public API
src/                     library: thread.c context.c scheduler.c timer.c
                         mutex.c semaphore.c condvar.c waitq.c trace.c internal.h
tests/                   T1-T11, run_tests.sh, run_valgrind.sh
apps/                    app_prodcons.c, app_mixed.c, app_workqueue.c, app_bank.c
bench/                   bench_switch.c, bench_sync.c, bench_scale.c,
                         run_benchmarks.sh, plot.py, results/
docs/                    figures/ (graphs), screenshots/, CHANGES.md
demo.sh                  live demonstration script
Makefile
```

## Known limitations

- Many-to-one: one blocking system call (for example `sleep` or a blocking `read`) stops every Weave thread (shown by T7), and Weave threads never run in parallel on several cores.
- `ITIMER_VIRTUAL` only counts the process's user CPU time and its resolution is the kernel tick (4 ms on the test machine), so a quantum shorter than one tick behaves like one tick.
- Each lock/unlock masks and unmasks `SIGVTALRM` with `sigprocmask` (two system calls), so an uncontended Weave mutex is slower than a futex-based pthread mutex.
- The Priority policy scans the whole ready queue on every switch (O(n)); Round Robin is O(1).
- Library calls are not async-signal-safe; do not call them from your own signal handlers. Priority inversion is not handled.

See `docs/CHANGES.md` for what changed since Phase 2.
