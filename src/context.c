/* ============================================================================
 * Module   : Context-Switch Layer (context.c)
 * Owner    : Renad Adel
 * Purpose  : Save and restore thread contexts and switch between threads.
 * OS concepts : Context switching with getcontext, makecontext and swapcontext
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * ========================================================================== */

#include "internal.h"

/* Purpose : Save the CPU context of 'from' and resume 'to'. The call returns
 *           only when 'from' is scheduled again.
 * Params  : from - the thread giving up the CPU; to - the thread that runs next. */
void ctx_switch(wv_tcb_t *from, wv_tcb_t *to)
{
    to->switches++;                            /* statistic: number of switches     */
    tcb_set_current(to);
    trace_event("switch to", to->id);
    swapcontext(&from->context, &to->context); /* save 'from', load 'to'            */
    thread_reap();                             /* free a finished thread's stack    */
}
