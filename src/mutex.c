/* ============================================================================
 * Module   : Mutex (mutex.c)
 * Owner    : Shatha Alharbi
 * Purpose  : Blocking mutex with owner tracking, error checks and a blocked queue.
 * OS concepts : Mutual exclusion, critical sections, blocked queues
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 *
 * Design:
 *  - A thread that finds the mutex taken is put in the mutex's FIFO queue and
 *    marked BLOCKED; it does not spin and does not use CPU time.
 *  - On unlock, ownership is HANDED OVER directly to the first waiting thread
 *    (the mutex stays locked). A newly arriving thread therefore cannot take
 *    the mutex before a thread that was already waiting, so waiters cannot
 *    starve, and a wake-up can never be lost or "stolen".
 *  - Every check-then-block step runs with SIGVTALRM masked (sched_lock), so a
 *    timer preemption cannot happen between "mutex is taken" and "I am queued".
 * ========================================================================== */

#include <stddef.h>
#include "internal.h"

/* Purpose : Prepare a mutex for use (unlocked, no owner, empty queue).
 * Params  : m - the mutex.
 * Returns : WV_OK, or WV_ERR_INVALID if m is NULL. */
int wv_mutex_init(wv_mutex_t *m)
{
    if (m == NULL) return WV_ERR_INVALID;
    m->locked = 0;
    m->owner = -1;
    m->blocked_queue = NULL;
    return WV_OK;
}

/* Purpose : Lock the mutex; block the caller in the mutex's queue if it is taken.
 *           When the call returns WV_OK, the caller owns the mutex.
 * Params  : m - the mutex.
 * Returns : WV_OK, or WV_ERR_INVALID if m is NULL, wv_init was not called, or
 *           the caller already owns the mutex (relocking would deadlock). */
int wv_mutex_lock(wv_mutex_t *m)
{
    wv_tcb_t *self = tcb_current();
    if (m == NULL || self == NULL) return WV_ERR_INVALID;

    sched_lock();
    if (m->locked && m->owner == self->id) {   /* would wait for itself forever */
        sched_unlock();
        return WV_ERR_INVALID;
    }
    if (!m->locked) {                          /* free: take it at once         */
        m->locked = 1;
        m->owner = self->id;
        sched_unlock();
        return WV_OK;
    }

    /* Taken: wait in FIFO order. wv_mutex_unlock makes us the owner before it
     * wakes us, so there is nothing to re-check after we run again. */
    waitq_push(&m->blocked_queue, self);
    trace_event("blocked (mutex)", self->id);
    sched_block(self);                         /* RUNNING -> BLOCKED            */
    sched_yield_to_next();
    sched_unlock();
    return WV_OK;
}

/* Purpose : Unlock the mutex and wake one blocked thread.
 *           If threads are waiting, the one that waited longest becomes the new
 *           owner and is moved to the ready queue; otherwise the mutex is freed.
 * Params  : m - the mutex.
 * Returns : WV_OK, WV_ERR_INVALID if m is NULL or wv_init was not called, or
 *           WV_ERR_NOTOWNER if the caller does not own the mutex (this includes
 *           unlocking a mutex that is not locked). */
int wv_mutex_unlock(wv_mutex_t *m)
{
    wv_tcb_t *self = tcb_current();
    if (m == NULL || self == NULL) return WV_ERR_INVALID;

    sched_lock();
    if (!m->locked || m->owner != self->id) {
        sched_unlock();
        return WV_ERR_NOTOWNER;
    }

    wv_tcb_t *next = waitq_pop(&m->blocked_queue);
    if (next != NULL) {                        /* hand ownership over           */
        m->owner = next->id;                   /* m->locked stays 1             */
        sched_wake(next);                      /* BLOCKED -> READY              */
        trace_event("ready", next->id);
    } else {
        m->locked = 0;
        m->owner = -1;
    }
    sched_unlock();
    return WV_OK;
}
