/* ============================================================================
 * Module   : Timer Preemption (timer.c)
 * Owner    : Nada Alrobaian
 * Purpose  : Virtual timer signal that forces a context switch, and signal masking for critical sections.
 * OS concepts : Timer interrupts and signals (setitimer, SIGVTALRM, sigprocmask)
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * ========================================================================== */

#include <signal.h>
#include <sys/time.h>
#include "internal.h"

/* Purpose : Mask SIGVTALRM so the scheduler cannot be interrupted. */
void sched_lock(void)   { /* TODO (Nada): sigprocmask(SIG_BLOCK, ...) */ }

/* Purpose : Unmask SIGVTALRM again. */
void sched_unlock(void) { /* TODO (Nada): sigprocmask(SIG_UNBLOCK, ...) */ }

/* TODO (Nada): timer_start(quantum_ms) with setitimer(ITIMER_VIRTUAL) and a SIGVTALRM
 * handler that moves the running thread to READY and calls sched_yield_to_next(). */
