/* ============================================================================
 * Module   : Tracing and Statistics (trace.c)
 * Owner    : Bana Jaber
 * Purpose  : Execution trace (which thread ran when) and per-thread statistics.
 * OS concepts : Scheduling metrics: waiting time, response time, run time, switch count
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * ========================================================================== */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "internal.h"

/* Statistics kept for one thread ID. The table is separate from the TCBs, so
 * the numbers survive after a thread has ended and its TCB has been freed. */
typedef struct {
    int  used;          /* 1 if this slot has been used by a thread           */
    long created_at;    /* time the thread was created (us since start)       */
    long first_run;     /* time it first got the CPU, or -1 if not yet        */
    long ready_since;   /* time it last became READY, or -1 if not READY      */
    long run_us;        /* total time spent RUNNING                           */
    long wait_us;       /* total time spent READY (waiting for the CPU)       */
    long switches;      /* number of times it was switched to                 */
    int  status;        /* ST_ACTIVE, ST_BLOCKED or ST_EXITED                 */
} wv_stat_t;

enum { ST_ACTIVE = 0, ST_BLOCKED = 1, ST_EXITED = 2 };

static int       trace_on = 0;
static int       started  = 0;       /* 1 after the first event              */
static long      t0;                 /* time of the first event              */
static wv_stat_t st[WV_MAX_THREADS];
static wv_tid_t  running  = 0;       /* thread that has the CPU now          */
static long      run_start;          /* time 'running' got the CPU           */

/* Purpose : Read the monotonic clock.
 * Returns : Microseconds since the first traced event. */
static long now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long us = ts.tv_sec * 1000000L + ts.tv_nsec / 1000;
    if (!started) {                  /* first call: start the clock at 0     */
        started = 1;
        t0 = us;
        run_start = 0;
        st[0].used = 1;              /* the main thread (ID 0) is running    */
        st[0].first_run = 0;
        st[0].ready_since = -1;
    }
    return us - t0;
}

/* Purpose : Switch the execution trace on or off.
 * Params  : on - 1 to print every event, 0 to stay silent. */
void wv_trace_enable(int on) { trace_on = on; }

/* Purpose : Record one scheduling event and update the statistics.
 *           Events sent by the library: "created", "ready", "switch to",
 *           "blocked (...)" and "exited".
 * Params  : what - name of the event; tid - thread it concerns. */
void trace_event(const char *what, wv_tid_t tid)
{
    long now = now_us();
    if (tid < 0 || tid >= WV_MAX_THREADS) return;
    wv_stat_t *s = &st[tid];

    if (strcmp(what, "created") == 0) {
        memset(s, 0, sizeof *s);
        s->used        = 1;
        s->created_at  = now;
        s->first_run   = -1;
        s->ready_since = now;        /* a new thread starts in the ready queue */
    }
    else if (strcmp(what, "ready") == 0) {
        s->status      = ST_ACTIVE;
        s->ready_since = now;
    }
    else if (strncmp(what, "blocked", 7) == 0) {
        s->status      = ST_BLOCKED; /* blocked time is not waiting time       */
        s->ready_since = -1;
    }
    else if (strcmp(what, "switch to") == 0) {
        wv_stat_t *old = &st[running];
        old->run_us += now - run_start;          /* close the old thread's slice */
        if (old->status == ST_ACTIVE && running != tid)
            old->ready_since = now;              /* it goes back to READY      */
        if (s->ready_since >= 0) s->wait_us += now - s->ready_since;
        s->ready_since = -1;
        s->status      = ST_ACTIVE;
        if (s->first_run < 0) s->first_run = now;
        s->switches++;
        running   = tid;
        run_start = now;
    }
    else if (strcmp(what, "exited") == 0) {
        s->status      = ST_EXITED;
        s->ready_since = -1;
    }

    if (trace_on) printf("[%8ld us] %-16s thread %d\n", now, what, tid);
}

/* Purpose : Print run time, waiting time, response time and number of
 *           switches for every thread that has existed. */
void wv_stats_print(void)
{
    long now = now_us();
    st[running].run_us += now - run_start;       /* count the current slice   */
    run_start = now;

    printf("\n%-6s %12s %12s %14s %9s\n",
           "Thread", "Run (us)", "Wait (us)", "Response (us)", "Switches");
    printf("------------------------------------------------------------\n");
    for (int i = 0; i < WV_MAX_THREADS; i++) {
        if (!st[i].used) continue;
        long resp = st[i].first_run >= 0 ? st[i].first_run - st[i].created_at : -1;
        printf("%-6d %12ld %12ld %14ld %9ld\n",
               i, st[i].run_us, st[i].wait_us, resp, st[i].switches);
    }
}
