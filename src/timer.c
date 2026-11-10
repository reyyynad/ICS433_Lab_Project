/* ============================================================================
 * Module   : Timer Preemption (timer.c)
 * Owner    : Nada Alrobaian
 * Purpose  : Virtual timer signal that forces a context switch, and signal
 *            masking for critical sections.
 * OS concepts : Timer interrupts and signals
 * Project  : Weave - User-Level Thread Library
 * ========================================================================== */

#include <signal.h>
#include <sys/time.h>

#include "internal.h"

/* Timer signal set used to protect scheduler critical sections. */
static sigset_t timer_signal_set;

/* Purpose : Handle the virtual timer expiration. */
static void timer_handler(int signal_number)
{
    (void)signal_number;

    /*
     * The timer quantum expired. The scheduler decides whether
     * another ready thread should run.
     */
    sched_yield_to_next();
}

/* Purpose : Mask SIGVTALRM so the scheduler cannot be interrupted. */
void sched_lock(void)
{
    sigemptyset(&timer_signal_set);
    sigaddset(&timer_signal_set, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &timer_signal_set, NULL);
}

/* Purpose : Unmask SIGVTALRM so timer preemption can continue. */
void sched_unlock(void)
{
    sigemptyset(&timer_signal_set);
    sigaddset(&timer_signal_set, SIGVTALRM);
    sigprocmask(SIG_UNBLOCK, &timer_signal_set, NULL);
}

/* Purpose : Install the timer handler and start the virtual timer. */
int timer_start(int quantum_ms)
{
    struct sigaction action;
    struct itimerval timer;

    if (quantum_ms <= 0) {
        return WV_ERR_INVALID;
    }

    action.sa_handler = timer_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;

    if (sigaction(SIGVTALRM, &action, NULL) != 0) {
        return WV_ERR_INVALID;
    }

    timer.it_value.tv_sec = quantum_ms / 1000;
    timer.it_value.tv_usec = (quantum_ms % 1000) * 1000;

    timer.it_interval.tv_sec = quantum_ms / 1000;
    timer.it_interval.tv_usec = (quantum_ms % 1000) * 1000;

    if (setitimer(ITIMER_VIRTUAL, &timer, NULL) != 0) {
        return WV_ERR_INVALID;
    }

    return WV_OK;
}
