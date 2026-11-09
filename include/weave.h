/* ============================================================================
 * Module   : Public API (weave.h)
 * Owner    : All members (API agreed in Week 1; change only after team approval)
 * Purpose  : Declares the functions, types and error codes seen by applications.
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * ========================================================================== */
#ifndef WEAVE_H
#define WEAVE_H

#include <stddef.h>

/* ---- Types ---------------------------------------------------------------- */
typedef int wv_tid_t;                       /* thread identifier                 */
typedef void *(*wv_func_t)(void *);         /* thread entry function             */

typedef enum { WV_POLICY_RR = 0, WV_POLICY_PRIORITY = 1 } wv_policy_t;

typedef enum { WV_NEW, WV_READY, WV_RUNNING, WV_BLOCKED, WV_TERMINATED } wv_state_t;

/* ---- Error codes (documented return values) ------------------------------- */
#define WV_OK            0
#define WV_ERR_NOTIMPL  (-1)   /* function not implemented yet (stub)           */
#define WV_ERR_INVALID  (-2)   /* invalid argument or thread ID                 */
#define WV_ERR_NOMEM    (-3)   /* stack or TCB allocation failed                */
#define WV_ERR_LIMIT    (-4)   /* maximum number of threads exceeded            */
#define WV_ERR_NOTOWNER (-5)   /* mutex unlocked by a thread that does not own it */
#define WV_MAX_THREADS  1024

/* ---- Synchronization objects (fields are filled in by Shatha) ------------- */
typedef struct wv_mutex { int locked; wv_tid_t owner; void *blocked_queue; } wv_mutex_t;
typedef struct wv_sem   { int value;  void *blocked_queue; }                 wv_sem_t;

/* ---- Thread lifecycle and scheduling API (Renad, Nada) -------------------- */
int      wv_init(wv_policy_t policy, int quantum_ms);
wv_tid_t wv_create(wv_func_t func, void *arg, int priority);
void     wv_yield(void);
void     wv_exit(void *retval);
int      wv_join(wv_tid_t tid, void **retval);
wv_tid_t wv_self(void);

/* ---- Synchronization API (Shatha) ----------------------------------------- */
int wv_mutex_init(wv_mutex_t *m);
int wv_mutex_lock(wv_mutex_t *m);
int wv_mutex_unlock(wv_mutex_t *m);
int wv_sem_init(wv_sem_t *s, int initial);
int wv_sem_wait(wv_sem_t *s);
int wv_sem_post(wv_sem_t *s);

/* ---- Tracing and statistics API (Bana) ------------------------------------ */
void wv_trace_enable(int on);               /* print every switch and state change */
void wv_stats_print(void);                  /* per-thread run, wait, response time */

#endif /* WEAVE_H */
