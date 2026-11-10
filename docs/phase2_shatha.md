# Phase 2 - Synchronization module (Shatha Alharbi, 202283660)

## Section 3 rows

| Module | Owner | Done (%) | Source files | What works now |
|---|---|---|---|---|
| Mutex (blocking, owner checks) | Shatha Alharbi | 100% | src/mutex.c, src/waitq.c | lock, unlock, FIFO blocked queue, ownership handed to the next waiter on unlock, owner checks (T5, T8 pass) |
| Semaphore (wait, post) | Shatha Alharbi | 100% | src/semaphore.c | wait and post with a FIFO blocked queue; bounded buffer (size 5, 3 producers, 3 consumers) consumes all 60 items exactly once (T6 pass; Valgrind clean) |
| Condition variables (extension) | Shatha Alharbi | 0% | src/condvar.c | planned for Week 6 |

## Screenshots
| Figure | Command | File |
|---|---|---|
| S7 | `./build/test_t5_race --no-lock` | docs/screenshots/S7_t5_race_no_lock.png |
| S8 | `./build/test_t5_race --lock` | docs/screenshots/S8_t5_race_lock.png |
| S9 | `./build/test_t6_semaphore` | docs/screenshots/S9_t6_semaphore.png |
| S10 | `./build/test_t8_errors` | docs/screenshots/S10_t8_errors.png |

## Section 6 - Challenges

| Challenge | How it was handled |
|---|---|
| 1. Lost wake-ups: a thread could find the mutex taken, get preempted before joining the queue, and miss the unlock that happened in between. | The check and the move into the blocked queue run with SIGVTALRM masked (`sched_lock`), so no switch can happen between them. |
| 2. A thread woken by unlock or post could lose the mutex or the semaphore unit to another thread that arrived later, and then block again. | Unlock gives ownership straight to the first waiter, and post gives its unit straight to the first waiter (the value is not increased). The woken thread does not compete again. |
| 3. Deciding which blocked thread wakes first. | Waiters are kept in FIFO order, so the thread that waited longest goes first and no waiter starves. The queue is a circular list stored as a single tail pointer, so it fits in the existing `blocked_queue` field of `weave.h` and both push and pop are O(1). |
| 4. Mutex ownership checks. | The owner's thread ID is stored in the mutex. Unlock by any other thread, or of a free mutex, returns `WV_ERR_NOTOWNER`. Locking a mutex the caller already holds returns `WV_ERR_INVALID` instead of deadlocking (T8). |
| 5. Showing the race in T5 before timer preemption was merged: with only cooperative switching, `counter++` never got interrupted. | The test reads the counter, forces `wv_yield()` every 1,000 increments, then writes. Without the mutex the final value is 1,000,000 instead of 4,000,000. With the mutex it is exactly 4,000,000. Once the timer is on, preemption can also occur at that point. |

## Notes for the team
- `sched_lock`/`sched_unlock` are still empty in `timer.c` (Nada). The mutex and semaphore already call them, so they become preemption-safe as soon as those two functions mask SIGVTALRM.
- Added `waitq_push`/`waitq_pop` to `src/internal.h` (new file `src/waitq.c`) and a comment describing the sync object fields in `include/weave.h`. No function signature or error code was changed.
