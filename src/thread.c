/* ============================================================================
 * Module   : Thread Manager (thread.c)
 * Owner    : Renad Adel
 * Purpose  : TCB table, wv_create, wv_exit, wv_join, wv_self, stack allocation and clean-up.
 * OS concepts : Threads and the TCB, thread states, memory management (private stacks)
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * ========================================================================== */

#include "internal.h"

/* Purpose : Start a new thread and place it in the READY state.
 * Params  : func - entry function; arg - its argument; priority - used by the Priority policy.
 * Returns : the new thread ID, or a negative WV_ERR_* code. */
wv_tid_t wv_create(wv_func_t func, void *arg, int priority)
{
    (void)func; (void)arg; (void)priority;
    /* TODO (Renad): allocate TCB and stack, makecontext, state NEW -> READY. */
    return WV_ERR_NOTIMPL;
}

/* Purpose : Terminate the calling thread, wake any joiner and free its stack. */
void wv_exit(void *retval)
{
    (void)retval;
    /* TODO (Renad): state -> TERMINATED, wake joiner, record statistics, schedule next. */
}

/* Purpose : Wait until thread tid terminates.
 * Returns : WV_OK, or WV_ERR_INVALID for an unknown ID. */
int wv_join(wv_tid_t tid, void **retval)
{
    (void)tid; (void)retval;
    /* TODO (Renad): block the caller until tid is TERMINATED. */
    return WV_ERR_NOTIMPL;
}

/* Purpose : Return the ID of the calling thread. */
wv_tid_t wv_self(void)
{
    /* TODO (Renad) */
    return WV_ERR_NOTIMPL;
}

/* Purpose : Give up the CPU voluntarily. */
void wv_yield(void)
{
    /* TODO (Renad): move the caller to READY and call sched_yield_to_next(). */
}

wv_tcb_t *tcb_current(void) { /* TODO (Renad) */ return NULL; }
wv_tcb_t *tcb_lookup(wv_tid_t tid) { (void)tid; /* TODO (Renad) */ return NULL; }
