/* ============================================================================
 * Module   : Example: worker pool with a job queue (app_workqueue.c)
 * Owner    : Shatha Alharbi
 * Purpose  : A small thread pool built only from Weave primitives.
 *            The main thread submits jobs into a bounded queue; 3 worker
 *            threads sleep on a condition variable until a job arrives.
 *            When all jobs are submitted, main sets 'shutdown' and broadcasts
 *            so every idle worker wakes up and exits. A counting semaphore
 *            limits how many workers may "use the printer" at the same time.
 * OS concepts : Condition variables (wait/signal/broadcast), mutex, counting
 *               semaphore as a resource limit, producer-consumer, clean shutdown
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * Usage    : ./build/app_workqueue [--trace] [--stats]
 * ========================================================================== */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../include/weave.h"

#define NWORKERS   3
#define NJOBS      12
#define QSIZE      4
#define PRINTERS   2            /* at most 2 workers print at the same time   */

static int        queue[QSIZE], qhead, qtail, qcount;
static int        shutdown_flag;
static int        done_by[NWORKERS];
static long       results[NJOBS];
static wv_mutex_t qm;
static wv_cond_t  has_job, has_space;
static wv_sem_t   printer;

/* Purpose : The "work": sum of squares 1..n, with a busy loop to use CPU time. */
static long work(int n)
{
    long s = 0;
    for (int i = 1; i <= n; i++) { s += (long)i * i; for (volatile int k = 0; k < 2000; k++) ; }
    return s;
}

/* Purpose : Worker loop: take a job, run it, report it; exit on shutdown. */
static void *worker(void *arg)
{
    int w = (int)(intptr_t)arg;
    for (;;) {
        wv_mutex_lock(&qm);
        while (qcount == 0 && !shutdown_flag) wv_cond_wait(&has_job, &qm);
        if (qcount == 0 && shutdown_flag) { wv_mutex_unlock(&qm); break; }
        int job = queue[qhead];
        qhead = (qhead + 1) % QSIZE;
        qcount--;
        wv_cond_signal(&has_space);            /* the submitter may continue   */
        wv_mutex_unlock(&qm);

        results[job] = work(1000 + job * 100); /* outside the lock             */
        done_by[w]++;

        wv_sem_wait(&printer);                 /* limited shared resource      */
        printf("  worker %d finished job %2d -> %ld\n", w, job, results[job]);
        wv_sem_post(&printer);
        wv_yield();
    }
    printf("  worker %d shutting down after %d jobs\n", w, done_by[w]);
    return NULL;
}

int main(int argc, char **argv)
{
    int stats = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) wv_trace_enable(1);
        if (strcmp(argv[i], "--stats") == 0) stats = 1;
    }
    if (wv_init(WV_POLICY_RR, 5) != WV_OK) { printf("wv_init failed\n"); return 1; }
    wv_mutex_init(&qm);
    wv_cond_init(&has_job);
    wv_cond_init(&has_space);
    wv_sem_init(&printer, PRINTERS);

    printf("Worker pool: %d workers, %d jobs, queue size %d\n", NWORKERS, NJOBS, QSIZE);
    wv_tid_t t[NWORKERS];
    for (int w = 0; w < NWORKERS; w++) t[w] = wv_create(worker, (void *)(intptr_t)w, 1);

    for (int j = 0; j < NJOBS; j++) {          /* main thread is the producer  */
        wv_mutex_lock(&qm);
        while (qcount == QSIZE) wv_cond_wait(&has_space, &qm);
        queue[qtail] = j;
        qtail = (qtail + 1) % QSIZE;
        qcount++;
        wv_cond_signal(&has_job);
        wv_mutex_unlock(&qm);
    }

    wv_mutex_lock(&qm);                        /* no more jobs: wake everyone  */
    shutdown_flag = 1;
    wv_cond_broadcast(&has_job);
    wv_mutex_unlock(&qm);
    for (int w = 0; w < NWORKERS; w++) wv_join(t[w], NULL);

    int total = 0, correct = 0;
    for (int w = 0; w < NWORKERS; w++) total += done_by[w];
    for (int j = 0; j < NJOBS; j++) {
        long n = 1000 + j * 100;
        correct += (results[j] == n * (n + 1) * (2 * n + 1) / 6);
    }
    printf("Jobs done: %d / %d, results correct: %d / %d\n", total, NJOBS, correct, NJOBS);
    if (stats) wv_stats_print();
    return (total == NJOBS && correct == NJOBS) ? 0 : 1;
}
