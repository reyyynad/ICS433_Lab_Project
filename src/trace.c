/* ============================================================================
 * Module   : Tracing and Statistics (trace.c)
 * Owner    : Bana Jaber
 * Purpose  : Execution trace (which thread ran when) and per-thread statistics.
 * OS concepts : Scheduling metrics: waiting time, response time, run time, switch count
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * ========================================================================== */

#include <stdio.h>
#include "internal.h"

static int trace_on = 0;

void wv_trace_enable(int on) { trace_on = on; }

/* Purpose : Log one scheduling event (context switch or state change). */
void trace_event(const char *what, wv_tid_t tid)
{
    if (trace_on) printf("[trace] %s thread %d\n", what, tid);
    /* TODO (Bana): add a timestamp and store the event for the final timeline. */
}

/* Purpose : Print run time, waiting time, response time and switches per thread. */
void wv_stats_print(void)
{
    /* TODO (Bana) */
}
