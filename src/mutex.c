/* ============================================================================
 * Module   : Mutex (mutex.c)
 * Owner    : Shatha Alharbi
 * Purpose  : Blocking mutex with owner tracking, error checks and a blocked queue.
 * OS concepts : Mutual exclusion, critical sections, blocked queues
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * ========================================================================== */

#include "internal.h"

int wv_mutex_init(wv_mutex_t *m)   { (void)m; /* TODO (Shatha) */ return WV_ERR_NOTIMPL; }

/* Purpose : Lock the mutex; block the caller in the mutex's queue if it is taken. */
int wv_mutex_lock(wv_mutex_t *m)   { (void)m; /* TODO (Shatha) */ return WV_ERR_NOTIMPL; }

/* Purpose : Unlock the mutex and wake one blocked thread.
 * Returns : WV_OK, or WV_ERR_NOTOWNER if the caller does not own the mutex. */
int wv_mutex_unlock(wv_mutex_t *m) { (void)m; /* TODO (Shatha) */ return WV_ERR_NOTIMPL; }
