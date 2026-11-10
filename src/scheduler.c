/* ============================================================================
 * Module   : Scheduler (scheduler.c)
 * Owner    : Nada Alrobaian
 * Purpose  : Ready queue, Round Robin scheduling and priority scheduling
 *            with aging.
 * OS concepts : CPU scheduling, ready queue, starvation and aging
 * Project  : Weave - User-Level Thread Library
 * ========================================================================== */

#include "internal.h"

/* Active scheduling settings. */
static wv_policy_t active_policy = WV_POLICY_RR;
static int quantum_ms = 10;

/* Ready queue pointers. */
static wv_tcb_t *ready_head = NULL;
static wv_tcb_t *ready_tail = NULL;

/* Add a thread to the end of the ready queue. */
static void ready_enqueue(wv_tcb_t *thread)
{
    if (thread == NULL) {
        return;
    }

    thread->next = NULL;

    if (ready_tail == NULL) {
        ready_head = thread;
        ready_tail = thread;
    } else {
        ready_tail->next = thread;
        ready_tail = thread;
    }
}

/* Remove and return the first thread in the ready queue. */
static wv_tcb_t *ready_dequeue(void)
{
    wv_tcb_t *thread;

    thread = ready_head;

    if (thread == NULL) {
        return NULL;
    }

    ready_head = thread->next;

    if (ready_head == NULL) {
        ready_tail = NULL;
    }

    thread->next = NULL;
    return thread;
}

/*
 * Give an extra priority boost to threads that were skipped.
 * The selected thread has already been removed from the ready queue,
 * so only waiting threads receive the aging increase.
 */
static void age_waiting_threads(void)
{
    wv_tcb_t *thread = ready_head;

    while (thread != NULL) {
        thread->priority++;
        thread = thread->next;
    }
}
/* Remove and return the ready thread with the highest priority. */
static wv_tcb_t *ready_remove_highest_priority(void)
{
    wv_tcb_t *best = NULL;
    wv_tcb_t *best_previous = NULL;
    wv_tcb_t *previous = NULL;
    wv_tcb_t *current = ready_head;

    while (current != NULL) {
        if (best == NULL || current->priority > best->priority) {
            best = current;
            best_previous = previous;
        }

        previous = current;
        current = current->next;
    }

    if (best == NULL) {
        return NULL;
    }

    if (best_previous == NULL) {
        ready_head = best->next;
    } else {
        best_previous->next = best->next;
    }

    if (best == ready_tail) {
        ready_tail = best_previous;
    }

    best->next = NULL;
    return best;
}

/*
 * Purpose : Initialize the library and start the timer.
 * Params  : policy - Round Robin or Priority scheduling.
 *           new_quantum_ms - timer quantum in milliseconds.
 * Returns : WV_OK or an error code.
 */
int wv_init(wv_policy_t policy, int new_quantum_ms)
{
    int result;

    if (policy != WV_POLICY_RR &&
        policy != WV_POLICY_PRIORITY) {
        return WV_ERR_INVALID;
    }

    if (new_quantum_ms <= 0) {
        return WV_ERR_INVALID;
    }

    active_policy = policy;
    quantum_ms = new_quantum_ms;

    ready_head = NULL;
    ready_tail = NULL;

    result = thread_init_main();

    if (result != WV_OK) {
        return result;
    }

    return timer_start(quantum_ms);
}

/*
 * Purpose : Add a thread to the ready queue.
 * Params  : thread - thread that is ready to run.
 */
void sched_ready(wv_tcb_t *thread)
{
    if (thread == NULL ||
        thread->state == WV_TERMINATED) {
        return;
    }

    thread->state = WV_READY;

    /* Bana uses this event to calculate waiting time. */
    trace_event("ready", thread->id);

    ready_enqueue(thread);
}

/*
 * Purpose : Mark a thread as blocked.
 * Params  : thread - thread that is waiting for a resource.
 */
void sched_block(wv_tcb_t *thread)
{
    if (thread == NULL) {
        return;
    }

    thread->state = WV_BLOCKED;
    thread->next = NULL;

    trace_event("blocked", thread->id);
}

/*
 * Purpose : Wake a blocked thread and return it to the ready queue.
 * Params  : thread - blocked thread to wake.
 */
void sched_wake(wv_tcb_t *thread)
{
    if (thread == NULL ||
        thread->state == WV_TERMINATED) {
        return;
    }

    sched_ready(thread);
}

/*
 * Purpose : Select the next thread and switch to it.
 *
 * Round Robin:
 * - Selects the first thread in the ready queue.
 *
 * Priority:
 * - Ages waiting threads.
 * - Selects the thread with the highest priority.
 */
void sched_yield_to_next(void)
{
    wv_tcb_t *current;
    wv_tcb_t *next;

    sched_lock();

    current = tcb_current();

    /*
     * The current running thread becomes READY when its
     * time quantum expires or when it voluntarily yields.
     */
    if (current != NULL &&
        current->state == WV_RUNNING) {
        sched_ready(current);
    }

    if (active_policy == WV_POLICY_PRIORITY) {
    /*
     * First select the highest-priority thread.
     * The selected thread is removed from the ready queue.
     */
    next = ready_remove_highest_priority();

    /*
     * Age only the threads that were skipped and remain waiting.
     */
    age_waiting_threads();
} else {
    next = ready_dequeue();
}

    /*
     * If no other thread is ready, continue running
     * the current thread.
     */
    if (next == NULL) {
        if (current != NULL &&
            current->state == WV_READY) {
            current->state = WV_RUNNING;
        }

        sched_unlock();
        return;
    }

    next->state = WV_RUNNING;
    trace_event("running", next->id);

    /*
     * Select the first thread if there is no current thread.
     * Otherwise, use Renad's context-switch function.
     */
    if (current == NULL) {
        tcb_set_current(next);
    } else if (current != next) {
        ctx_switch(current, next);
    }

    sched_unlock();
}
