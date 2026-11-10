/* ============================================================================
 * Test     : T5 - Race condition with and without mutex
 * Owner    : Shatha Alharbi
 * Exit code: 0 = PASS, 1 = FAIL, 2 = NOT IMPLEMENTED YET
 * Expected : 4 threads each add 1 to a shared counter 1,000,000 times.
 *            Without a mutex, updates are lost and the final counter is below
 *            4,000,000. With the mutex, the final counter is exactly 4,000,000.
 * Usage    : ./build/test_t5_race            (runs both cases, used by make test)
 *            ./build/test_t5_race --no-lock  (only the unsafe version)
 *            ./build/test_t5_race --lock     (only the mutex version)
 *
 * How the race is produced: counter++ is split into read, (possible switch),
 * write. A thread may lose the CPU between reading and writing the counter.
 * Until timer preemption is merged, the switch is forced with wv_yield() every
 * YIELD_EVERY increments; with preemption on, the timer can also switch there.
 * Either way the other threads see the old value and their increments are lost.
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include "../include/weave.h"

#define NTHREADS     4
#define INCREMENTS   1000000
#define EXPECTED     ((long)NTHREADS * INCREMENTS)
#define YIELD_EVERY  1000            /* force a switch inside the critical section */

static volatile long counter;
static wv_mutex_t    lock;
static int           use_lock;

/* Purpose : Add 1 to the shared counter INCREMENTS times (read, switch, write). */
static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < INCREMENTS; i++) {
        if (use_lock) wv_mutex_lock(&lock);
        long tmp = counter;                       /* read                       */
        if (i % YIELD_EVERY == 0) wv_yield();     /* lose the CPU in the middle */
        counter = tmp + 1;                        /* write (may overwrite others) */
        if (use_lock) wv_mutex_unlock(&lock);
    }
    return NULL;
}

/* Purpose : Run one experiment and return the final counter value. */
static long run(int with_lock)
{
    wv_tid_t t[NTHREADS];
    counter = 0;
    use_lock = with_lock;
    for (int i = 0; i < NTHREADS; i++) t[i] = wv_create(worker, NULL, 1);
    for (int i = 0; i < NTHREADS; i++) wv_join(t[i], NULL);
    return counter;
}

int main(int argc, char **argv)
{
    int do_nolock = 1, do_lock = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-lock") == 0) do_lock = 0;
        else if (strcmp(argv[i], "--lock") == 0) do_nolock = 0;
        else if (strcmp(argv[i], "--trace") == 0) wv_trace_enable(1);
    }
    if (wv_init(WV_POLICY_RR, 10) != WV_OK) { printf("T5 FAIL: wv_init\n"); return 1; }
    if (wv_mutex_init(&lock) != WV_OK)       { printf("T5 FAIL: wv_mutex_init\n"); return 1; }

    printf("T5 Race condition: %d threads x %d increments, expected %ld\n",
           NTHREADS, INCREMENTS, EXPECTED);
    int ok = 1;

    if (do_nolock) {
        long c = run(0);
        printf("  WITHOUT mutex: final counter = %ld (lost updates = %ld)\n", c, EXPECTED - c);
        if (c < EXPECTED) printf("  -> race condition shown: updates were lost\n");
        else { printf("  -> no lost updates (race was not triggered)\n"); ok = 0; }
    }
    if (do_lock) {
        long c = run(1);
        printf("  WITH mutex   : final counter = %ld (lost updates = %ld)\n", c, EXPECTED - c);
        if (c == EXPECTED) printf("  -> correct: the mutex protects the critical section\n");
        else ok = 0;
    }
    printf(ok ? "T5 PASS\n" : "T5 FAIL\n");
    return ok ? 0 : 1;
}
