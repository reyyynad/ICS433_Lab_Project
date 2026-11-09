/* ============================================================================
 * Module   : Scheduler (scheduler.c)
 * Owner    : Nada Alrobaian
 * Purpose  : Ready queue, pluggable policy interface, Round Robin and Priority with aging.
 * OS concepts : CPU scheduling, ready queue, starvation and aging
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * ========================================================================== */

#include "internal.h"

/* Purpose : Start the library, choose the policy and time quantum.
 * Params  : policy - WV_POLICY_RR or WV_POLICY_PRIORITY; quantum_ms - time slice.
 * Returns : WV_OK or a negative WV_ERR_* code. */
int wv_init(wv_policy_t policy, int quantum_ms)
{
    (void)policy; (void)quantum_ms;
    /* TODO (Nada + Renad): create the main thread's TCB, select policy, start the timer. */
    return WV_ERR_NOTIMPL;
}

void sched_ready(wv_tcb_t *t) { (void)t; /* TODO (Nada): enqueue by policy */ }
void sched_block(wv_tcb_t *t) { (void)t; /* TODO (Nada) */ }
void sched_wake(wv_tcb_t *t)  { (void)t; /* TODO (Nada) */ }

/* Purpose : Pick the next thread according to the active policy and switch to it. */
void sched_yield_to_next(void)
{
    /* TODO (Nada): Policy 1 Round Robin; Policy 2 Priority with aging. */
}
