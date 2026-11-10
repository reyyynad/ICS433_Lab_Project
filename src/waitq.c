/* ============================================================================
 * Module   : Synchronization Wait Queue (waitq.c)
 * Owner    : Shatha Alharbi
 * Purpose  : FIFO queue of blocked threads shared by the mutex and the semaphore.
 * OS concepts : Blocked (waiting) queues, FIFO wake-up order, starvation freedom
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * ========================================================================== */

#include <stddef.h>
#include "internal.h"

/* The queue is a circular singly linked list. *q points to the LAST thread,
 * and last->next is the FIRST thread. An empty queue is *q == NULL.
 * The caller must hold sched_lock() so the timer cannot interrupt an update. */

/* Purpose : Add a thread at the back of the queue.
 * Params  : q - address of the queue pointer; t - thread to add. */
void waitq_push(void **q, wv_tcb_t *t)
{
    wv_tcb_t *tail = (wv_tcb_t *)*q;
    if (tail == NULL) {
        t->next = t;                 /* one element: it points to itself      */
    } else {
        t->next = tail->next;        /* new tail points to the head           */
        tail->next = t;
    }
    *q = t;                          /* t is the new tail                     */
}

/* Purpose : Remove the thread at the front of the queue.
 * Params  : q - address of the queue pointer.
 * Returns : the thread that waited longest, or NULL if the queue is empty. */
wv_tcb_t *waitq_pop(void **q)
{
    wv_tcb_t *tail = (wv_tcb_t *)*q;
    if (tail == NULL) return NULL;

    wv_tcb_t *head = tail->next;
    if (head == tail) *q = NULL;     /* it was the only element               */
    else tail->next = head->next;
    head->next = NULL;
    return head;
}
