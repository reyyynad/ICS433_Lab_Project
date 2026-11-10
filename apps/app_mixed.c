/* ============================================================================
 * Module   : Example: mixed CPU-bound and interactive workload (app_mixed.c)
 * Owner    : Bana Jaber
 * Purpose  : Shows scheduling behaviour and prints the trace and statistics.
 *            Two CPU-bound threads never give up the CPU on their own and are
 *            preempted by the timer. Two interactive threads do a tiny amount
 *            of work and then yield, as if waiting for the next key press.
 *            The statistics show that the interactive threads still get a
 *            short response time even though the CPU-bound threads never yield.
 * OS concepts : Scheduling policies, preemption, response time, fairness
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * Usage    : ./build/app_mixed [--trace] [--stats] [--priority]
 * ========================================================================== */

#include <stdio.h>
#include <string.h>
#include "../include/weave.h"

#define QUANTUM_MS   10
#define CPU_CHUNKS   6            /* CPU-bound work, in chunks               */
#define CHUNK_LOOPS  25000000L    /* busy loop iterations per chunk          */
#define KEY_PRESSES  4            /* interactive "events" per thread         */

typedef struct { const char *name; int interactive; } job_t;

/* Purpose : Burn CPU time without yielding (the timer must preempt it). */
static void busy(long loops)
{
    for (volatile long i = 0; i < loops; i++) ;
}

/* Purpose : Thread body for both kinds of job. */
static void *job(void *arg)
{
    job_t *j = arg;
    if (j->interactive) {
        for (int k = 1; k <= KEY_PRESSES; k++) {
            busy(CHUNK_LOOPS / 1000);                /* very short burst     */
            printf("  %-6s handled event %d\n", j->name, k);
            wv_yield();                               /* wait for next event  */
        }
    } else {
        for (int c = 1; c <= CPU_CHUNKS; c++) {
            busy(CHUNK_LOOPS);                        /* long burst, no yield */
            printf("  %-6s finished chunk %d/%d\n", j->name, c, CPU_CHUNKS);
        }
    }
    printf("  %-6s done\n", j->name);
    return NULL;
}

int main(int argc, char **argv)
{
    int trace = 0, stats = 0;
    wv_policy_t policy = WV_POLICY_RR;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--trace")    == 0) trace = 1;
        else if (strcmp(argv[i], "--stats")    == 0) stats = 1;
        else if (strcmp(argv[i], "--priority") == 0) policy = WV_POLICY_PRIORITY;
        else { printf("usage: %s [--trace] [--stats] [--priority]\n", argv[0]); return 1; }
    }

    if (wv_init(policy, QUANTUM_MS) != WV_OK) { printf("wv_init failed\n"); return 1; }
    if (trace) wv_trace_enable(1);

    static job_t jobs[] = {
        { "cpu-A", 0 }, { "cpu-B", 0 }, { "ui-1", 1 }, { "ui-2", 1 },
    };
    /* Under the Priority policy the interactive threads get a higher priority. */
    int prio[] = { 1, 1, 5, 5 };
    int n = sizeof jobs / sizeof jobs[0];
    wv_tid_t tids[4];

    printf("Mixed workload: 2 CPU-bound + 2 interactive threads, %s, quantum %d ms\n",
           policy == WV_POLICY_RR ? "Round Robin" : "Priority with aging", QUANTUM_MS);
    for (int i = 0; i < n; i++) {
        tids[i] = wv_create(job, &jobs[i], prio[i]);
        if (tids[i] < 0) { printf("wv_create failed (%d)\n", tids[i]); return 1; }
    }
    for (int i = 0; i < n; i++) wv_join(tids[i], NULL);

    if (stats) {
        printf("\nThread IDs:");
        for (int i = 0; i < n; i++) printf("  %d = %s", tids[i], jobs[i].name);
        printf("  (0 = main)\n");
        wv_stats_print();
    }
    return 0;
}
