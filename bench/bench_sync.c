/* ============================================================================
 * Module   : Benchmark: synchronization cost, Weave vs pthreads (bench_sync.c)
 * Owner    : Shatha Alharbi, Bana Jaber
 * Purpose  : Measures the cost (microseconds per operation) of:
 *            1. Uncontended lock + unlock         (wv_mutex   vs pthread_mutex)
 *            2. Semaphore hand-off between 2 threads (wv_sem  vs POSIX sem_t)
 *            3. Condition-variable hand-off between 2 threads
 *                                                 (wv_cond    vs pthread_cond)
 *            4. Contended mutex: 4 threads, each 50,000 lock/unlock pairs
 *            A "hand-off" means thread A wakes thread B and then waits until
 *            B wakes it back, so it includes one block, one wake-up and one
 *            context switch.
 * OS concepts : Cost of blocking synchronization, user-level vs kernel-level
 *               (futex) blocking, context-switch overhead
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * Usage    : ./build/bench_sync [--csv]
 * ========================================================================== */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include "../include/weave.h"

#define N_UNCONT   1000000   /* lock/unlock pairs, uncontended                */
#define N_HANDOFF  100000    /* hand-offs between two threads                 */
#define N_CONT     50000     /* lock/unlock pairs per thread, contended       */
#define N_CONT_THR 4
#define RUNS       5

static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

static double best_of(double (*bench)(void))
{
    double best = bench();
    for (int r = 1; r < RUNS; r++) { double v = bench(); if (v < best) best = v; }
    return best;
}

/* ---------------------------------------------------------------- Weave --- */
static wv_mutex_t wm;
static wv_sem_t   ws_a, ws_b;
static wv_cond_t  wc;
static int        wturn;
static volatile long wshared;

static double weave_uncontended(void)
{
    wv_mutex_init(&wm);
    double t = now_us();
    for (int i = 0; i < N_UNCONT; i++) { wv_mutex_lock(&wm); wv_mutex_unlock(&wm); }
    return (now_us() - t) / N_UNCONT;
}

static void *wsem_a(void *x) { (void)x; for (int i = 0; i < N_HANDOFF / 2; i++) { wv_sem_post(&ws_b); wv_sem_wait(&ws_a); } return NULL; }
static void *wsem_b(void *x) { (void)x; for (int i = 0; i < N_HANDOFF / 2; i++) { wv_sem_wait(&ws_b); wv_sem_post(&ws_a); } return NULL; }

static double weave_sem_handoff(void)
{
    wv_sem_init(&ws_a, 0); wv_sem_init(&ws_b, 0);
    double t = now_us();
    wv_tid_t a = wv_create(wsem_a, NULL, 1), b = wv_create(wsem_b, NULL, 1);
    wv_join(a, NULL); wv_join(b, NULL);
    return (now_us() - t) / N_HANDOFF;
}

static void *wcond_pp(void *arg)
{
    int me = *(int *)arg;
    for (int i = 0; i < N_HANDOFF / 2; i++) {
        wv_mutex_lock(&wm);
        while (wturn != me) wv_cond_wait(&wc, &wm);
        wturn = 1 - me;
        wv_cond_signal(&wc);
        wv_mutex_unlock(&wm);
    }
    return NULL;
}

static double weave_cond_handoff(void)
{
    static int ida = 0, idb = 1;
    wv_mutex_init(&wm); wv_cond_init(&wc); wturn = 0;
    double t = now_us();
    wv_tid_t a = wv_create(wcond_pp, &ida, 1), b = wv_create(wcond_pp, &idb, 1);
    wv_join(a, NULL); wv_join(b, NULL);
    return (now_us() - t) / N_HANDOFF;
}

static void *wcont(void *x)
{
    (void)x;
    for (int i = 0; i < N_CONT; i++) {
        wv_mutex_lock(&wm); wshared++;
        if (i % 64 == 0) wv_yield();          /* hold the lock across a switch */
        wv_mutex_unlock(&wm);
    }
    return NULL;
}

static double weave_contended(void)
{
    wv_mutex_init(&wm); wshared = 0;
    wv_tid_t t[N_CONT_THR];
    double s = now_us();
    for (int i = 0; i < N_CONT_THR; i++) t[i] = wv_create(wcont, NULL, 1);
    for (int i = 0; i < N_CONT_THR; i++) wv_join(t[i], NULL);
    return (now_us() - s) / (N_CONT * N_CONT_THR);
}

/* ------------------------------------------------------------- pthreads --- */
static pthread_mutex_t pm = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  pc = PTHREAD_COND_INITIALIZER;
static sem_t           ps_a, ps_b;
static int             pturn;
static volatile long   pshared;

static double pthread_uncontended(void)
{
    double t = now_us();
    for (int i = 0; i < N_UNCONT; i++) { pthread_mutex_lock(&pm); pthread_mutex_unlock(&pm); }
    return (now_us() - t) / N_UNCONT;
}

static void *psem_a(void *x) { (void)x; for (int i = 0; i < N_HANDOFF / 2; i++) { sem_post(&ps_b); sem_wait(&ps_a); } return NULL; }
static void *psem_b(void *x) { (void)x; for (int i = 0; i < N_HANDOFF / 2; i++) { sem_wait(&ps_b); sem_post(&ps_a); } return NULL; }

static double pthread_sem_handoff(void)
{
    sem_init(&ps_a, 0, 0); sem_init(&ps_b, 0, 0);
    pthread_t a, b;
    double t = now_us();
    pthread_create(&a, NULL, psem_a, NULL); pthread_create(&b, NULL, psem_b, NULL);
    pthread_join(a, NULL); pthread_join(b, NULL);
    double r = (now_us() - t) / N_HANDOFF;
    sem_destroy(&ps_a); sem_destroy(&ps_b);
    return r;
}

static void *pcond_pp(void *arg)
{
    int me = *(int *)arg;
    for (int i = 0; i < N_HANDOFF / 2; i++) {
        pthread_mutex_lock(&pm);
        while (pturn != me) pthread_cond_wait(&pc, &pm);
        pturn = 1 - me;
        pthread_cond_signal(&pc);
        pthread_mutex_unlock(&pm);
    }
    return NULL;
}

static double pthread_cond_handoff(void)
{
    static int ida = 0, idb = 1;
    pthread_t a, b; pturn = 0;
    double t = now_us();
    pthread_create(&a, NULL, pcond_pp, &ida); pthread_create(&b, NULL, pcond_pp, &idb);
    pthread_join(a, NULL); pthread_join(b, NULL);
    return (now_us() - t) / N_HANDOFF;
}

static void *pcont(void *x)
{
    (void)x;
    for (int i = 0; i < N_CONT; i++) {
        pthread_mutex_lock(&pm); pshared++;
        if (i % 64 == 0) sched_yield();
        pthread_mutex_unlock(&pm);
    }
    return NULL;
}

static double pthread_contended(void)
{
    pthread_t t[N_CONT_THR]; pshared = 0;
    double s = now_us();
    for (int i = 0; i < N_CONT_THR; i++) pthread_create(&t[i], NULL, pcont, NULL);
    for (int i = 0; i < N_CONT_THR; i++) pthread_join(t[i], NULL);
    return (now_us() - s) / (N_CONT * N_CONT_THR);
}

/* ----------------------------------------------------------------- main --- */
int main(int argc, char **argv)
{
    int csv = (argc > 1 && strcmp(argv[1], "--csv") == 0);
    if (wv_init(WV_POLICY_RR, 10) != WV_OK) { printf("wv_init failed\n"); return 1; }

    struct { const char *key, *label; double (*w)(void), (*p)(void); } b[] = {
        { "mutex_uncontended", "Mutex lock+unlock (no contention)", weave_uncontended,  pthread_uncontended },
        { "sem_handoff",       "Semaphore hand-off (2 threads)",    weave_sem_handoff,  pthread_sem_handoff },
        { "cond_handoff",      "Condvar hand-off (2 threads)",      weave_cond_handoff, pthread_cond_handoff },
        { "mutex_contended",   "Mutex, 4 threads contending",       weave_contended,    pthread_contended },
    };
    int n = sizeof b / sizeof b[0];
    double wr[8], pr[8];
    for (int i = 0; i < n; i++) { wr[i] = best_of(b[i].w); pr[i] = best_of(b[i].p); }

    if (csv) {
        printf("metric,weave_us,pthreads_us\n");
        for (int i = 0; i < n; i++) printf("%s,%.4f,%.4f\n", b[i].key, wr[i], pr[i]);
        return 0;
    }
    printf("Synchronization cost: Weave vs pthreads (best of %d runs, microseconds per operation)\n\n", RUNS);
    printf("%-36s %10s %10s %9s\n", "Operation", "Weave", "pthreads", "Ratio");
    printf("--------------------------------------------------------------------\n");
    for (int i = 0; i < n; i++)
        printf("%-36s %10.4f %10.4f %8.1fx\n", b[i].label, wr[i], pr[i], pr[i] / wr[i]);
    printf("\nRatio = pthreads time / Weave time (above 1 means Weave is faster).\n");
    return 0;
}
