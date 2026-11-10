/* ============================================================================
 * Module   : Counting Semaphore (semaphore.c)
 * Owner    : Shatha Alharbi
 * Purpose  : Counting semaphore with wait and post and a blocked queue.
 * OS concepts : Semaphores, producer-consumer synchronization
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 *
 * Design:
 *  - value is never negative; it counts the free units.
 *  - wv_sem_wait takes a unit if one is free, otherwise the caller blocks in
 *    the FIFO queue.
 *  - wv_sem_post gives the unit DIRECTLY to the first waiting thread (value is
 *    not incremented), so the woken thread does not have to compete for it
 *    again. This prevents lost wake-ups and keeps the wake-up order FIFO.
 *  - Both operations run with SIGVTALRM masked (sched_lock).
 * ========================================================================== */

#include <stddef.h>
#include "internal.h"

/* Purpose : Prepare a semaphore with a starting number of units.
 * Params  : s - the semaphore; initial - starting value (must be >= 0).
 * Returns : WV_OK, or WV_ERR_INVALID if s is NULL or initial is negative. */
int wv_sem_init(wv_sem_t *s, int initial)
{
    if (s == NULL || initial < 0) return WV_ERR_INVALID;
    s->value = initial;
    s->blocked_queue = NULL;
    return WV_OK;
}

/* Purpose : Take one unit (P / down). Blocks while no unit is free.
 * Params  : s - the semaphore.
 * Returns : WV_OK, or WV_ERR_INVALID if s is NULL or wv_init was not called. */
int wv_sem_wait(wv_sem_t *s)
{
    wv_tcb_t *self = tcb_current();
    if (s == NULL || self == NULL) return WV_ERR_INVALID;

    sched_lock();
    if (s->value > 0) {                        /* a unit is free: take it       */
        s->value--;
        sched_unlock();
        return WV_OK;
    }

    /* No unit: wait. wv_sem_post hands its unit to us before waking us. */
    /* sched_block first: it may reset self->next, which the queue uses. */
    sched_block(self);                         /* RUNNING -> BLOCKED            */
    waitq_push(&s->blocked_queue, self);
    trace_event("blocked (sem)", self->id);
    sched_yield_to_next();
    sched_unlock();
    return WV_OK;
}

/* Purpose : Release one unit (V / up). Wakes the thread that waited longest,
 *           or adds the unit to value if nobody is waiting. Never blocks.
 * Params  : s - the semaphore.
 * Returns : WV_OK, or WV_ERR_INVALID if s is NULL or wv_init was not called. */
int wv_sem_post(wv_sem_t *s)
{
    if (s == NULL || tcb_current() == NULL) return WV_ERR_INVALID;

    sched_lock();
    wv_tcb_t *next = waitq_pop(&s->blocked_queue);
    if (next != NULL) {                        /* hand the unit over            */
        sched_wake(next);                      /* BLOCKED -> READY              */
        trace_event("ready", next->id);
    } else {
        s->value++;
    }
    sched_unlock();
    return WV_OK;
}
