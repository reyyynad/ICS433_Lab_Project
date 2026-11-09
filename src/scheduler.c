/* ============================================================================
 * Module   : Scheduler (scheduler.c)
 * Owner    : Nada Alrobaian
 * Purpose  : Ready queue, pluggable policy interface, Round Robin and Priority with aging.
 * OS concepts : CPU scheduling, ready queue, starvation and aging
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * ========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include "internal.h"

/* ---- BASIC FIFO READY QUEUE -------------------------------------------------
 * Written by Renad so that threads can run and T1/T2 can be tested.
 * TODO (Nada): replace with the pluggable policy interface, Round Robin with a
 * time quantum and Priority with aging (see wv_init below and timer.c). */
static wv_tcb_t *q_head = NULL, *q_tail = NULL;

static void enqueue(wv_tcb_t *t)
{
    t->next = NULL;
    if (q_tail) q_tail->next = t; else q_head = t;
    q_tail = t;
}

static wv_tcb_t *dequeue(void)
{
    wv_tcb_t *t = q_head;
    if (t) { q_head = t->next; if (!q_head) q_tail = NULL; t->next = NULL; }
    return t;
}

/* Purpose : Start the library, choose the policy and time quantum.
 * Params  : policy - WV_POLICY_RR or WV_POLICY_PRIORITY; quantum_ms - time slice.
 * Returns : WV_OK or a negative WV_ERR_* code. */
int wv_init(wv_policy_t policy, int quantum_ms)
{
    (void)policy; (void)quantum_ms;
    /* TODO (Nada): store the policy and quantum, start the virtual timer. */
    return thread_init_main();                 /* TCB of the main thread (Renad)    */
}

/* Purpose : Put a thread in the ready queue (state READY). */
void sched_ready(wv_tcb_t *t) { t->state = WV_READY; enqueue(t); }

/* Purpose : Mark a thread BLOCKED. The queue it waits in belongs to the mutex,
 *           semaphore or joined thread. */
void sched_block(wv_tcb_t *t) { t->state = WV_BLOCKED; }

/* Purpose : Move a blocked thread back to the ready queue. */
void sched_wake(wv_tcb_t *t)  { sched_ready(t); }

/* Purpose : Pick the next thread and switch to it. If no thread can run and the
 *           caller cannot continue, the program is deadlocked and ends. */
void sched_yield_to_next(void)
{
    wv_tcb_t *cur  = tcb_current();
    wv_tcb_t *next = dequeue();                /* TODO (Nada): ask the active policy */

    if (next == NULL) {
        if (cur->state == WV_RUNNING) return;  /* nobody else wants the CPU         */
        fprintf(stderr, "[weave] deadlock: no runnable threads\n");
        exit(1);
    }
    next->state = WV_RUNNING;
    if (next == cur) return;                   /* yielded to ourselves              */
    ctx_switch(cur, next);
}
