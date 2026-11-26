/* ============================================================================
 * Test     : T11 - Synchronization under timer preemption, both policies
 * Owner    : Shatha Alharbi (integration test with Nada's scheduler)
 * Exit code: 0 = PASS, 1 = FAIL, 2 = NOT IMPLEMENTED YET
 * Expected : The threads never call wv_yield; every switch comes from the
 *            SIGVTALRM timer (quantum 1 ms). With Round Robin and again with
 *            Priority + aging (threads of priority 1..6):
 *              1. 6 threads x 200,000 locked increments -> counter exactly 1,200,000
 *              2. a semaphore initialised to 2 never lets more than 2 threads
 *                 into its section at once, and every thread finishes
 *              3. the timer really interleaved the threads: at least one thread saw
 *                 other threads' increments between its own first and last one
 *            Each policy runs in its own child process because wv_init can be
 *            called once per process.
 * Usage    : ./build/test_t11_sync_policies
 * ========================================================================== */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../include/weave.h"

#define NTHREADS   6
#define INCREMENTS 200000
#define SEM_UNITS  2
#define SEM_ROUNDS 300

static volatile long counter;
static wv_mutex_t    m;
static wv_sem_t      s;
static volatile int  inside, max_inside, finished, interleaved;

/* Purpose : Locked read-modify-write with a small delay inside, no yields. */
static void *worker(void *arg)
{
    (void)arg;
    long first = -1, last = -1;
    for (int i = 0; i < INCREMENTS; i++) {
        wv_mutex_lock(&m);
        long tmp = counter;
        if (first < 0) first = tmp;
        last = tmp;
        for (volatile int k = 0; k < 20; k++) ;   /* timer can fire here */
        counter = tmp + 1;
        wv_mutex_unlock(&m);
    }
    if (last - first + 1 > INCREMENTS) interleaved = 1;   /* others ran in between */
    for (int r = 0; r < SEM_ROUNDS; r++) {
        wv_sem_wait(&s);
        wv_mutex_lock(&m);
        if (++inside > max_inside) max_inside = inside;
        wv_mutex_unlock(&m);
        for (volatile int k = 0; k < 20000; k++) ;
        wv_mutex_lock(&m);
        inside--;
        wv_mutex_unlock(&m);
        wv_sem_post(&s);
    }
    wv_mutex_lock(&m); finished++; wv_mutex_unlock(&m);
    return NULL;
}

/* Purpose : Run the workload under one policy. Returns 0 on success. */
static int run_policy(wv_policy_t pol, const char *name)
{
    if (wv_init(pol, 1) != WV_OK) { printf("  %s: wv_init failed\n", name); return 1; }
    wv_mutex_init(&m);
    wv_sem_init(&s, SEM_UNITS);
    wv_tid_t t[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) t[i] = wv_create(worker, NULL, i + 1);
    for (int i = 0; i < NTHREADS; i++) wv_join(t[i], NULL);

    long expected = (long)NTHREADS * INCREMENTS;
    int ok = (counter == expected) && (max_inside <= SEM_UNITS) && (finished == NTHREADS) && interleaved;
    printf("  %-17s counter %ld / %ld | max in semaphore %d (limit %d) | finished %d/%d | "
           "preempted: %s -> %s\n", name, counter, expected, max_inside, SEM_UNITS,
           finished, NTHREADS, interleaved ? "yes" : "no", ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

int main(void)
{
    printf("T11 Synchronization under timer-only preemption (quantum 1 ms)\n");
    fflush(stdout);
    int fails = 0;
    struct { wv_policy_t p; const char *n; } cases[] = {
        { WV_POLICY_RR, "Round Robin" }, { WV_POLICY_PRIORITY, "Priority + aging" } };
    for (int i = 0; i < 2; i++) {
        pid_t pid = fork();
        if (pid == 0) { int rc = run_policy(cases[i].p, cases[i].n); fflush(stdout); _exit(rc); }
        int st = 0;
        waitpid(pid, &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) fails++;
    }
    printf(fails == 0 ? "T11 PASS\n" : "T11 FAIL\n");
    return fails == 0 ? 0 : 1;
}
