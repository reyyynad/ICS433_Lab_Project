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
    int            vg_stack_id;    /* Valgrind stack registration (debugging only) */
    void          *retval;
    wv_tid_t       joiner;         /* thread waiting in wv_join, or -1          */
    wv_func_t      entry;          /* thread function and its argument          */
    void          *arg;
    struct wv_tcb *next;           /* link used by the ready and blocked queues */
    /* statistics (Bana reads these) */
    long           run_us, wait_us, switches;
} wv_tcb_t;

/* ---- Thread Manager (Renad) ---- */
int       thread_init_main(void);           /* TCB for the main thread (id 0)     */
wv_tcb_t *tcb_current(void);                /* the running thread                 */
void      tcb_set_current(wv_tcb_t *t);
wv_tcb_t *tcb_lookup(wv_tid_t tid);
void      thread_reap(void);                /* free the stack of a finished thread */

/* ---- Context-switch layer (Renad) ---- */
void ctx_switch(wv_tcb_t *from, wv_tcb_t *to);

/* ---- Scheduler (Nada) ---- */
void sched_ready(wv_tcb_t *t);              /* put a thread in the ready queue    */
void sched_block(wv_tcb_t *t);              /* move running thread to blocked     */
void sched_yield_to_next(void);             /* pick next thread and switch to it  */
void sched_wake(wv_tcb_t *t);               /* blocked -> ready                   */
void sched_lock(void);                      /* mask SIGVTALRM (critical section)  */
void sched_unlock(void);                    /* unmask SIGVTALRM                   */

/* ---- Synchronization wait queues (Shatha) ----
 * FIFO queue of blocked threads, linked through wv_tcb_t.next. The queue is a
 * circular list and is stored as one pointer to its LAST element (tail->next is
 * the head), so it fits in the 'void *blocked_queue' field of wv_mutex_t and
 * wv_sem_t and both operations are O(1). A blocked thread is never in the ready
 * queue at the same time, so sharing the 'next' link is safe. */
void      waitq_push(void **q, wv_tcb_t *t);  /* add t at the back of the queue */
wv_tcb_t *waitq_pop(void **q);                 /* remove the front, or NULL      */

/* ---- Tracing hooks (Bana) ---- */
void trace_event(const char *what, wv_tid_t tid);

#endif /* WEAVE_INTERNAL_H */
