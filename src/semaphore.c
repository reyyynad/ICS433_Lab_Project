/* ============================================================================
 * Module   : Counting Semaphore (semaphore.c)
 * Owner    : Shatha Alharbi
 * Purpose  : Counting semaphore with wait and post and a blocked queue.
 * OS concepts : Semaphores, producer-consumer synchronization
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * ========================================================================== */

#include "internal.h"

int wv_sem_init(wv_sem_t *s, int initial) { (void)s; (void)initial; /* TODO (Shatha) */ return WV_ERR_NOTIMPL; }
int wv_sem_wait(wv_sem_t *s)              { (void)s; /* TODO (Shatha) */ return WV_ERR_NOTIMPL; }
int wv_sem_post(wv_sem_t *s)              { (void)s; /* TODO (Shatha) */ return WV_ERR_NOTIMPL; }
