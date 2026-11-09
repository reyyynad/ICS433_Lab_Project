/* ============================================================================
 * Module   : Internal shared declarations (internal.h)
 * Owner    : Renad Adel (TCB), Nada Alrobaian (queues); agree changes together
 * Purpose  : TCB layout and helper functions shared by the library modules.
 *            Applications must NOT include this file.
 * OS concepts : Thread Control Block, ready queue, blocked queue
 * ========================================================================== */
#ifndef WEAVE_INTERNAL_H
#define WEAVE_INTERNAL_H

#include <ucontext.h>
#include "../include/weave.h"

/* Thread Control Block: everything the library knows about one thread. */
typedef struct wv_tcb {
    wv_tid_t       id;
    wv_state_t     state;
    int            priority;
    ucontext_t     context;        /* saved CPU registers                        */
    void          *stack;          /* private stack, freed when thread ends     */
    size_t         stack_size;
    void          *retval;
    wv_tid_t       joiner;         /* thread waiting in wv_join, or -1          */
    struct wv_tcb *next;           /* link used by the ready and blocked queues */
    /* statistics (Bana reads these) */
    long           run_us, wait_us, switches;
} wv_tcb_t;

/* ---- Thread Manager (Renad) ---- */
wv_tcb_t *tcb_current(void);
wv_tcb_t *tcb_lookup(wv_tid_t tid);

/* ---- Context-switch layer (Renad) ---- */
void ctx_switch(wv_tcb_t *from, wv_tcb_t *to);

/* ---- Scheduler (Nada) ---- */
void sched_ready(wv_tcb_t *t);              /* put a thread in the ready queue    */
void sched_block(wv_tcb_t *t);              /* move running thread to blocked     */
void sched_yield_to_next(void);             /* pick next thread and switch to it  */
void sched_wake(wv_tcb_t *t);               /* blocked -> ready                   */
void sched_lock(void);                      /* mask SIGVTALRM (critical section)  */
void sched_unlock(void);                    /* unmask SIGVTALRM                   */

/* ---- Tracing hooks (Bana) ---- */
void trace_event(const char *what, wv_tid_t tid);

#endif /* WEAVE_INTERNAL_H */
