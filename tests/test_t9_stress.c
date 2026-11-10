/* ============================================================================
 * Test     : T9 - Stress (500 threads, one mutex)
 * Owner    : Bana Jaber
 * Exit code: 0 = PASS, 1 = FAIL, 2 = NOT IMPLEMENTED YET
 * Expected : 500 threads run at the same time under preemptive Round Robin
 *            and all update one shared counter protected by one mutex. The
 *            final counter is exactly 500 x INCREMENTS, every thread finishes
 *            and is joined, there is no deadlock and no crash.
 * Usage    : ./build/test_t9_stress [--trace]
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "../include/weave.h"

#define NTHREADS    500
#define INCREMENTS  200          /* per thread                                 */
#define QUANTUM_MS  1            /* short quantum = many preemptions           */
#define TIMEOUT_SEC 60           /* longer than this means a deadlock          */

static wv_mutex_t lock;
static long       counter  = 0;   /* protected by 'lock'                       */
static int        finished = 0;   /* threads that completed, under 'lock'      */
static int        in_cs    = 0;   /* threads inside the critical section       */
static int        overlap  = 0;   /* set if two threads were ever inside       */

/* Purpose : Called by the watchdog alarm if the test does not end in time. */
static void on_timeout(int sig)
{
    (void)sig;
    const char msg[] = "T9 FAIL: timeout, possible deadlock\n";
    write(1, msg, sizeof msg - 1);
    _exit(1);
}

/* Purpose : Each worker increments the shared counter INCREMENTS times.
 *           The read-modify-write is spread out and sometimes yields inside
 *           the critical section, so other threads really do find the mutex
 *           taken and have to block on it. */
static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < INCREMENTS; i++) {
        wv_mutex_lock(&lock);
        if (++in_cs > 1) overlap = 1;      /* mutual exclusion check          */

        long v = counter;
        for (volatile int k = 0; k < 50; k++) ;   /* widen the race window    */
        if (i % 50 == 0) wv_yield();       /* hold the mutex across a switch  */
        counter = v + 1;

        in_cs--;
        wv_mutex_unlock(&lock);
    }
    wv_mutex_lock(&lock);
    finished++;
    wv_mutex_unlock(&lock);
    return NULL;
}

int main(int argc, char **argv)
{
    signal(SIGALRM, on_timeout);
    alarm(TIMEOUT_SEC);

    if (wv_init(WV_POLICY_RR, QUANTUM_MS) != WV_OK) { printf("T9 FAIL: wv_init\n"); return 1; }
    if (argc > 1 && strcmp(argv[1], "--trace") == 0) wv_trace_enable(1);
    wv_mutex_init(&lock);

    printf("T9 Stress: %d threads x %d increments, one mutex, RR quantum %d ms\n",
           NTHREADS, INCREMENTS, QUANTUM_MS);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    wv_tid_t tids[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        tids[i] = wv_create(worker, NULL, 1);
        if (tids[i] < 0) {
            printf("T9 FAIL: wv_create returned %d for thread %d\n", tids[i], i);
            return 1;
        }
    }
    int join_errors = 0;
    for (int i = 0; i < NTHREADS; i++)
        if (wv_join(tids[i], NULL) != WV_OK) join_errors++;

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    long expected = (long)NTHREADS * INCREMENTS;

    printf("  threads created and joined : %d (join errors: %d)\n", NTHREADS, join_errors);
    printf("  threads that finished      : %d\n", finished);
    printf("  final counter              : %ld (expected %ld)\n", counter, expected);
    printf("  mutual exclusion violated  : %s\n", overlap ? "YES" : "no");
    printf("  time                       : %.3f s\n", secs);

    int ok = (counter == expected && finished == NTHREADS &&
              join_errors == 0 && !overlap);
    printf(ok ? "T9 PASS (no lost updates, no deadlock, no crash)\n" : "T9 FAIL\n");
    return ok ? 0 : 1;
}
