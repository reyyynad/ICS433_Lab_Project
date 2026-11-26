# Changes since Phase 2 (final phase)

## New features
- **Condition variables** (`src/condvar.c`, Shatha): `wv_cond_init`, `wv_cond_wait`,
  `wv_cond_signal`, `wv_cond_broadcast`, `wv_cond_destroy`. Mesa semantics, FIFO wake-up,
  and `wv_cond_wait` releases the mutex and blocks in one critical section, so no wake-up is lost.
  This was the planned extension (0 % in Phase 2).
- **Non-blocking and clean-up calls** (Shatha): `wv_mutex_trylock`, `wv_mutex_destroy`,
  `wv_sem_trywait`, `wv_sem_destroy`, and the new error code `WV_ERR_BUSY (-6)`.
- **Worker-pool application** `apps/app_workqueue.c`: 3 workers, a bounded job queue,
  condition variables for "job available" / "space available", broadcast on shutdown, and a
  semaphore limiting access to a shared resource.

- **Bank-account race demonstration** `apps/app_bank.c` (from the proposal): 8 tellers, 5 accounts;
  without locking the total money changes, with one mutex per account (locked in a fixed order) it
  is conserved.
- **Priority without aging** (`WV_POLICY_PRIORITY_NOAGING`, scheduler): used only to show
  starvation. T4 now runs both cases, as planned in the proposal's test table.

## Refinements and bug fixes
- `mutex.c`: the release logic is now one internal function, `mutex_release_locked`, shared by
  `wv_mutex_unlock` and `wv_cond_wait`.
- `mutex.c`, `semaphore.c`: removed a duplicate `"ready"` trace event (the scheduler already
  records it in `sched_ready`).
- **Use-after-free at program exit (found with Valgrind in T3)**: a timer tick that arrived during
  `exit()` could run the scheduler after `thread_cleanup` had freed the TCBs. `timer.c` now has
  `timer_stop()`, called first in `thread_cleanup`. All 11 tests and 3 apps are now Valgrind clean.
- `test_t2_yield.c`: uses a 1,000 ms quantum. The test checks `wv_yield` only, and under Valgrind
  (about 50 times slower) a timer tick could reorder the two threads.
- `tests/run_tests.sh`: runs the tests in numeric order (T1 ... T11).

## New tests
- **T10** `test_t10_condvar`: monitor bounded buffer (4 producers, 4 consumers, 400 items, 1 ms
  quantum), signal wakes exactly one waiter in FIFO order, broadcast wakes all, plus 12 error and
  non-blocking checks (21 checks in total).
- **T11** `test_t11_sync_policies`: mutex and semaphore with timer-only preemption (no
  `wv_yield`), under Round Robin and under Priority with aging.

## Evaluation
- `bench_scale --policies`: average waiting, response and completion time under RR, Priority + aging
  and Priority without aging (proposal metric: waiting/response/completion per policy, starvation check).
- `bench/bench_sync.c`: mutex (uncontended and 4-thread contended), semaphore hand-off and
  condition-variable hand-off, Weave vs pthreads.
- `bench/bench_scale.c`: switch cost for 2–1,000 ready threads (RR vs Priority) and a time-quantum
  study (1–100 ms).
- `bench/run_benchmarks.sh` + `bench/plot.py`: one command writes the CSV results and the graphs.
- `make valgrind`, `make bench`, `make demo` targets; `tests/run_valgrind.sh`; `demo.sh`.

## API changes (all additions; nothing from Phase 2 was removed or renamed)
`wv_cond_t`, `WV_ERR_BUSY`, and the ten functions listed above.
