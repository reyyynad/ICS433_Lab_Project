/* ============================================================================
 * Module   : Condition Variables (condvar.c)  - final-phase extension
 * Owner    : Shatha Alharbi
 * Purpose  : wait / signal / broadcast on a condition, always used together
 *            with a Weave mutex (monitor-style synchronization).
 * OS concepts : Condition variables, monitors, Mesa semantics, atomic
 *               "release the lock and sleep", lost wake-up prevention
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 *
 * Design:
 *  - Waiters are kept in the same O(1) FIFO wait queue used by the mutex and
 *    the semaphore (src/waitq.c), so the longest-waiting thread wakes first.
 *  - wv_cond_wait releases the mutex and blocks in ONE critical section
 *    (SIGVTALRM masked). A signal sent by another thread can therefore never
 *    fall between "mutex released" and "I am in the queue" (no lost wake-up).
 *  - Mesa semantics: a woken thread is only moved to the ready queue. It must
 *    lock the mutex again before wv_cond_wait returns, and the condition may
 *    have changed by then, so callers re-check it in a while loop:
 *        wv_mutex_lock(&m);
 *        while (!condition) wv_cond_wait(&c, &m);
 *        ... use the shared state ...
 *        wv_mutex_unlock(&m);
 *  - Signal and broadcast with no waiters do nothing (signals are not stored,
 *    unlike a semaphore post).
 * ========================================================================== */

#include <stddef.h>
#include "internal.h"

/* Purpose : Prepare a condition variable for use (no waiters).
 * Params  : c - the condition variable.
 * Returns : WV_OK, or WV_ERR_INVALID if c is NULL. */
int wv_cond_init(wv_cond_t *c)
{
    if (c == NULL) return WV_ERR_INVALID;
    c->blocked_queue = NULL;
    return WV_OK;
}

/* Purpose : Atomically release mutex m and block until the condition is
 *           signalled; then lock m again before returning.
 * Params  : c - the condition variable; m - a mutex the caller owns.
 * Returns : WV_OK (caller owns m again), WV_ERR_INVALID if c or m is NULL or
 *           wv_init was not called, or WV_ERR_NOTOWNER if the caller does not
 *           own m (waiting would then break the mutex). */
int wv_cond_wait(wv_cond_t *c, wv_mutex_t *m)
{
    wv_tcb_t *self = tcb_current();
    if (c == NULL || m == NULL || self == NULL) return WV_ERR_INVALID;

    sched_lock();
    if (!m->locked || m->owner != self->id) {
        sched_unlock();
        return WV_ERR_NOTOWNER;
    }

    mutex_release_locked(m);                   /* 1. give the mutex away        */
    sched_block(self);                         /* 2. RUNNING -> BLOCKED (resets  */
    waitq_push(&c->blocked_queue, self);       /*    next, so push afterwards)  */
    trace_event("blocked (cond)", self->id);
    sched_yield_to_next();                     /* 3. sleep; unlocks on return   */

    /* Woken by signal/broadcast: compete for the mutex again (Mesa semantics). */
    return wv_mutex_lock(m);
}

/* Purpose : Wake the thread that has waited longest on c, if any.
 * Params  : c - the condition variable.
 * Returns : WV_OK, or WV_ERR_INVALID if c is NULL or wv_init was not called. */
int wv_cond_signal(wv_cond_t *c)
{
    if (c == NULL || tcb_current() == NULL) return WV_ERR_INVALID;
    sched_lock();
    wv_tcb_t *t = waitq_pop(&c->blocked_queue);
    if (t != NULL) sched_wake(t);              /* BLOCKED -> READY              */
    sched_unlock();
    return WV_OK;
}

/* Purpose : Wake every thread waiting on c, in FIFO order.
 * Params  : c - the condition variable.
 * Returns : WV_OK, or WV_ERR_INVALID if c is NULL or wv_init was not called. */
int wv_cond_broadcast(wv_cond_t *c)
{
    if (c == NULL || tcb_current() == NULL) return WV_ERR_INVALID;
    sched_lock();
    wv_tcb_t *t;
    while ((t = waitq_pop(&c->blocked_queue)) != NULL) sched_wake(t);
    sched_unlock();
    return WV_OK;
}

/* Purpose : Check that no thread is waiting on c and reset it.
 * Params  : c - the condition variable.
 * Returns : WV_OK, WV_ERR_INVALID if c is NULL, or WV_ERR_BUSY if threads are
 *           still waiting. */
int wv_cond_destroy(wv_cond_t *c)
{
    if (c == NULL) return WV_ERR_INVALID;
    sched_lock();
    int busy = c->blocked_queue != NULL;
    sched_unlock();
    return busy ? WV_ERR_BUSY : WV_OK;
}
