/* ============================================================================
 * Module   : Context-Switch Layer (context.c)
 * Owner    : Renad Adel
 * Purpose  : Save and restore thread contexts and switch between them.
 * OS concepts : Context switching with getcontext, makecontext and swapcontext
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * ========================================================================== */

#include "internal.h"

/* Purpose : Save the CPU context of 'from' and resume 'to'. */
void ctx_switch(wv_tcb_t *from, wv_tcb_t *to)
{
    (void)from; (void)to;
    /* TODO (Renad): swapcontext(&from->context, &to->context); count the switch. */
}
