/* ============================================================================
 * Module   : Benchmark: creation and context-switch time (bench_switch.c)
 * Owner    : Bana Jaber
 * Purpose  : Measures Weave against pthreads (microseconds).
 *            1. Creation: create and join N threads, time per thread.
 *            2. Switch  : two threads hand the CPU back and forth; time per switch.
 *               Weave uses wv_yield; pthreads uses a mutex and condition variable.
 * OS concepts : Context-switch overhead, user-level vs kernel-level threads
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * Usage    : ./build/bench_switch [--csv]
 * ========================================================================== */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "../include/weave.h"

#define N_CREATE   1000      /* threads created in the creation benchmark     */
#define N_SWITCH   100000    /* hand-overs in the switch benchmark            */
#define RUNS       5         /* each benchmark is repeated; the best run counts */

/* Purpose : Read the monotonic clock.
 * Returns : Microseconds as a double. */
static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

/* ---------------------------------------------------------------- Weave --- */

static void *wv_empty(void *arg) { return arg; }

/* Purpose : Time to create and join one Weave thread (average over N_CREATE). */
static double weave_create(void)
{
    double t = now_us();
    for (int i = 0; i < N_CREATE; i++) {
        wv_tid_t tid = wv_create(wv_empty, NULL, 1);
        wv_join(tid, NULL);
    }
    return (now_us() - t) / N_CREATE;
}

static void *wv_pingpong(void *arg)
{
    (void)arg;
    for (int i = 0; i < N_SWITCH / 2; i++) wv_yield();
    return NULL;
}

/* Purpose : Time of one Weave context switch (two threads calling wv_yield). */
static double weave_switch(void)
{
    double t = now_us();
    wv_tid_t a = wv_create(wv_pingpong, NULL, 1);
    wv_tid_t b = wv_create(wv_pingpong, NULL, 1);
    wv_join(a, NULL);
    wv_join(b, NULL);
    return (now_us() - t) / N_SWITCH;
}

/* ------------------------------------------------------------- pthreads --- */

static void *pt_empty(void *arg) { return arg; }

/* Purpose : Time to create and join one pthread (average over N_CREATE). */
static double pthread_create_time(void)
{
    double t = now_us();
    for (int i = 0; i < N_CREATE; i++) {
        pthread_t th;
        pthread_create(&th, NULL, pt_empty, NULL);
        pthread_join(th, NULL);
    }
    return (now_us() - t) / N_CREATE;
}

static pthread_mutex_t pm   = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  pc   = PTHREAD_COND_INITIALIZER;
static int             turn = 0;          /* whose turn it is: 0 or 1         */

static void *pt_pingpong(void *arg)
{
    int me = *(int *)arg;
    for (int i = 0; i < N_SWITCH / 2; i++) {
        pthread_mutex_lock(&pm);
        while (turn != me) pthread_cond_wait(&pc, &pm);
        turn = 1 - me;                       /* hand the turn to the other one */
        pthread_cond_signal(&pc);
        pthread_mutex_unlock(&pm);
    }
    return NULL;
}

/* Purpose : Time of one pthread hand-over (kernel switch plus mutex/condvar). */
static double pthread_switch(void)
{
    pthread_t a, b;
    int ida = 0, idb = 1;
    turn = 0;
    double t = now_us();
    pthread_create(&a, NULL, pt_pingpong, &ida);
    pthread_create(&b, NULL, pt_pingpong, &idb);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    return (now_us() - t) / N_SWITCH;
}

/* ----------------------------------------------------------------- main --- */

/* Purpose : Run a benchmark RUNS times and keep the fastest result, which is
 *           the one least disturbed by other activity on the machine. */
static double best_of(double (*bench)(void))
{
    double best = bench();
    for (int r = 1; r < RUNS; r++) {
        double v = bench();
        if (v < best) best = v;
    }
    return best;
}

int main(int argc, char **argv)
{
    int csv = (argc > 1 && strcmp(argv[1], "--csv") == 0);
    if (wv_init(WV_POLICY_RR, 10) != WV_OK) { printf("wv_init failed\n"); return 1; }

    double wc = best_of(weave_create),  pc_ = best_of(pthread_create_time);
    double ws = best_of(weave_switch),  ps  = best_of(pthread_switch);

    if (csv) {
        printf("metric,weave_us,pthreads_us\n");
        printf("create_join,%.3f,%.3f\n", wc, pc_);
        printf("switch,%.3f,%.3f\n", ws, ps);
        return 0;
    }
    printf("Weave vs pthreads (best of %d runs, microseconds)\n\n", RUNS);
    printf("%-26s %12s %12s %10s\n", "Operation", "Weave", "pthreads", "Speed-up");
    printf("----------------------------------------------------------------\n");
    printf("%-26s %12.3f %12.3f %9.1fx\n", "Create + join one thread", wc, pc_, pc_ / wc);
    printf("%-26s %12.3f %12.3f %9.1fx\n", "One context switch", ws, ps, ps / ws);
    printf("\n(%d threads created; %d switches measured)\n", N_CREATE, N_SWITCH);
    return 0;
}
