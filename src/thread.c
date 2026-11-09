/* ============================================================================
 * Module   : Thread Manager (thread.c)
 * Owner    : Renad Adel
 * Purpose  : TCB table, wv_create, wv_exit, wv_join, wv_self, wv_yield,
 *            stack allocation and clean-up of terminated threads.
 * OS concepts : Threads and the TCB, thread states, memory management
 *               (a private stack for every thread)
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * ========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include "internal.h"

/* Valgrind cannot tell that we switch between our own stacks and reports false
 * errors. When valgrind.h is installed, every stack is registered with it. These
 * macros do nothing when the header is missing, so the library builds anywhere. */
#if defined(__has_include)
#  if __has_include(<valgrind/valgrind.h>)
#    include <valgrind/valgrind.h>
#  endif
#endif
#ifndef VALGRIND_STACK_REGISTER
#  define VALGRIND_STACK_REGISTER(lo, hi) 0
#  define VALGRIND_STACK_DEREGISTER(id)   ((void)(id))
#endif

#define STACK_SIZE (64 * 1024)          /* private stack of every thread (bytes) */

static wv_tcb_t *table[WV_MAX_THREADS]; /* TCB table: the slot number is the thread ID */
static wv_tcb_t *current = NULL;        /* the thread that is running now        */
static wv_tcb_t *zombie  = NULL;        /* finished thread whose stack is not yet freed */

/* ---- Helpers --------------------------------------------------------------- */

wv_tcb_t *tcb_current(void)            { return current; }
void      tcb_set_current(wv_tcb_t *t) { current = t; }

/* Purpose : Find a thread by ID.
 * Returns : its TCB, or NULL if the ID is invalid or the thread no longer exists. */
wv_tcb_t *tcb_lookup(wv_tid_t tid)
{
    if (tid < 0 || tid >= WV_MAX_THREADS) return NULL;
    return table[tid];
}

/* Purpose : Find the lowest free slot in the TCB table (IDs are reused after join).
 * Returns : the slot number, or -1 if the table is full. */
static int find_free_slot(void)
{
    for (int i = 0; i < WV_MAX_THREADS; i++)
        if (table[i] == NULL) return i;
    return -1;
}

/* Purpose : Free everything still allocated when the program ends, so that
 *           Valgrind reports no memory leaks. The stack we are running on is skipped. */
static void thread_cleanup(void)
{
    for (int i = 0; i < WV_MAX_THREADS; i++) {
        wv_tcb_t *t = table[i];
        if (t == NULL || t == current) continue;
        VALGRIND_STACK_DEREGISTER(t->vg_stack_id);
        free(t->stack);
        free(t);
        table[i] = NULL;
    }
    if (current != NULL && current->id == 0) {   /* main thread runs on the normal stack */
        free(current);
        table[0] = NULL;
        current  = NULL;
    }
}

/* Purpose : Free the stack of a thread that has terminated. This cannot be done
 *           inside wv_exit (the thread is still running on that stack), so it is
 *           done right after the next context switch, by the next running thread. */
void thread_reap(void)
{
    if (zombie != NULL && zombie != current) {
        VALGRIND_STACK_DEREGISTER(zombie->vg_stack_id);
        free(zombie->stack);
        zombie->stack = NULL;
        zombie = NULL;
    }
}

/* Purpose : Create the TCB of the main thread (ID 0). Called by wv_init.
 * Returns : WV_OK, or a negative WV_ERR_* code. */
int thread_init_main(void)
{
    if (current != NULL) return WV_ERR_INVALID;          /* already initialised */
    wv_tcb_t *m = calloc(1, sizeof *m);
    if (m == NULL) return WV_ERR_NOMEM;
    m->id = 0;
    m->state = WV_RUNNING;
    m->joiner = -1;
    table[0] = m;
    current  = m;
    atexit(thread_cleanup);
    return WV_OK;
}

/* Purpose : First code run by every new thread: call the user function, then
 *           terminate the thread with its return value. */
static void thread_entry(void)
{
    thread_reap();
    wv_tcb_t *t = current;
    void *result = t->entry(t->arg);
    wv_exit(result);
}

/* ---- Public API ------------------------------------------------------------- */

/* Purpose : Start a new thread and place it in the READY state.
 * Params  : func - entry function; arg - its argument; priority - used by the Priority policy.
 * Returns : the new thread ID, or a negative WV_ERR_* code. */
wv_tid_t wv_create(wv_func_t func, void *arg, int priority)
{
    if (current == NULL || func == NULL) return WV_ERR_INVALID;   /* wv_init not called */

    int id = find_free_slot();
    if (id < 0) return WV_ERR_LIMIT;

    wv_tcb_t *t = calloc(1, sizeof *t);
    if (t == NULL) return WV_ERR_NOMEM;
    t->stack = malloc(STACK_SIZE);
    if (t->stack == NULL) { free(t); return WV_ERR_NOMEM; }

    t->id = id;
    t->state = WV_NEW;
    t->priority = priority;
    t->stack_size = STACK_SIZE;
    t->joiner = -1;
    t->entry = func;
    t->arg = arg;

    t->vg_stack_id = VALGRIND_STACK_REGISTER(t->stack, (char *)t->stack + t->stack_size);
    getcontext(&t->context);                   /* start from a valid context        */
    t->context.uc_stack.ss_sp = t->stack;      /* give the thread its private stack */
    t->context.uc_stack.ss_size = t->stack_size;
    t->context.uc_link = NULL;                 /* thread_entry never returns        */
    makecontext(&t->context, thread_entry, 0);

    table[id] = t;
    trace_event("created", id);
    sched_ready(t);                            /* NEW -> READY                      */
    return id;
}

/* Purpose : Terminate the calling thread, wake a thread waiting in wv_join,
 *           and switch to the next thread. Never returns.
 * Note    : The main thread must return from main() instead; if it calls
 *           wv_exit the whole program ends. */
void wv_exit(void *retval)
{
    wv_tcb_t *t = current;
    if (t == NULL) return;
    if (t->id == 0) exit(0);

    t->retval = retval;
    t->state = WV_TERMINATED;
    trace_event("exited", t->id);
    zombie = t;                                /* stack is freed after the switch   */

    if (t->joiner >= 0) {                      /* wake the thread waiting for us    */
        wv_tcb_t *j = tcb_lookup(t->joiner);
        if (j != NULL) sched_wake(j);
    }
    sched_yield_to_next();
    abort();                                   /* not reached                       */
}

/* Purpose : Wait until thread tid terminates and collect its return value.
 * Returns : WV_OK, or WV_ERR_INVALID for an unknown ID, joining yourself, or
 *           a thread that is already being joined. */
int wv_join(wv_tid_t tid, void **retval)
{
    wv_tcb_t *self = current;
    wv_tcb_t *t = tcb_lookup(tid);
    if (self == NULL || t == NULL || t == self || t->joiner >= 0) return WV_ERR_INVALID;

    if (t->state != WV_TERMINATED) {           /* block until the target exits      */
        t->joiner = self->id;
        trace_event("blocked (join)", self->id);
        sched_block(self);                     /* RUNNING -> BLOCKED                */
        sched_yield_to_next();
    }
    if (retval != NULL) *retval = t->retval;
    table[tid] = NULL;                         /* the TCB is no longer needed       */
    free(t);
    return WV_OK;
}

/* Purpose : Return the ID of the calling thread (or WV_ERR_INVALID before wv_init). */
wv_tid_t wv_self(void)
{
    return (current != NULL) ? current->id : WV_ERR_INVALID;
}

/* Purpose : Give up the CPU voluntarily: RUNNING -> READY, then run the next thread. */
void wv_yield(void)
{
    if (current == NULL) return;
    sched_ready(current);
    sched_yield_to_next();
}
