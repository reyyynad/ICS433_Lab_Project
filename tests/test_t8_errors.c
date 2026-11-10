/* ============================================================================
 * Test     : T8 - Error cases return documented codes
 * Owner    : Shatha Alharbi
 * Exit code: 0 = PASS, 1 = FAIL, 2 = NOT IMPLEMENTED YET
 * Expected : Each wrong call returns its documented WV_ERR_* code, and the
 *            library keeps working afterwards (a normal thread still runs).
 * Usage    : ./build/test_t8_errors
 * ========================================================================== */
#include <stdio.h>
#include "../include/weave.h"

static int passed = 0, failed = 0;

/* Purpose : Compare a return value with the expected code and print the result. */
static void check(const char *what, int got, int expected)
{
    int ok = (got == expected);
    printf("  [%s] %-46s got %3d, expected %3d\n", ok ? "ok  " : "FAIL", what, got, expected);
    if (ok) passed++; else failed++;
}

static wv_mutex_t m;

static void *idle(void *arg)        { (void)arg; return NULL; }
static void *lock_and_exit(void *arg) { (void)arg; wv_mutex_lock(&m); return (void *)42; }
static void *try_unlock(void *arg)  { (void)arg; return (void *)(long)wv_mutex_unlock(&m); }

int main(void)
{
    printf("T8 Error cases: each call must return its documented error code\n");

    /* Before wv_init */
    check("wv_create before wv_init", wv_create(idle, NULL, 1), WV_ERR_INVALID);

    if (wv_init(WV_POLICY_RR, 10) != WV_OK) { printf("T8 FAIL: wv_init\n"); return 1; }

    /* Invalid arguments */
    check("wv_create with NULL function", wv_create(NULL, NULL, 1), WV_ERR_INVALID);
    check("wv_join on an ID that does not exist", wv_join(77, NULL), WV_ERR_INVALID);
    check("wv_join on a negative ID", wv_join(-3, NULL), WV_ERR_INVALID);
    check("wv_join on itself", wv_join(wv_self(), NULL), WV_ERR_INVALID);
    check("wv_mutex_init(NULL)", wv_mutex_init(NULL), WV_ERR_INVALID);
    wv_sem_t s;
    check("wv_sem_init with negative value", wv_sem_init(&s, -1), WV_ERR_INVALID);
    check("wv_sem_wait(NULL)", wv_sem_wait(NULL), WV_ERR_INVALID);

    /* Mutex ownership */
    wv_mutex_init(&m);
    check("unlock of a mutex that is not locked", wv_mutex_unlock(&m), WV_ERR_NOTOWNER);
    wv_tid_t t = wv_create(lock_and_exit, NULL, 1);
    void *rv = NULL;
    wv_join(t, &rv);                         /* thread t now owns m              */
    check("unlock of a mutex owned by another thread", wv_mutex_unlock(&m), WV_ERR_NOTOWNER);

    wv_mutex_init(&m);
    wv_mutex_lock(&m);                       /* main owns m                      */
    t = wv_create(try_unlock, NULL, 1);
    wv_join(t, &rv);
    check("unlock by a thread that is not the owner", (int)(long)rv, WV_ERR_NOTOWNER);
    check("lock a mutex the caller already owns", wv_mutex_lock(&m), WV_ERR_INVALID);
    check("unlock by the owner (valid call)", wv_mutex_unlock(&m), WV_OK);

    /* Too many threads: main is ID 0, so IDs 1..1023 fill the table. */
    static wv_tid_t ids[WV_MAX_THREADS];
    int created = 0;
    for (;;) {
        wv_tid_t id = wv_create(idle, NULL, 1);
        if (id < 0) { check("wv_create when the thread table is full", id, WV_ERR_LIMIT); break; }
        ids[created++] = id;
    }
    printf("         (%d threads created before the limit was reached)\n", created);
    for (int i = 0; i < created; i++) wv_join(ids[i], NULL);

    /* The library must still work after all the errors. */
    t = wv_create(lock_and_exit, NULL, 1);
    wv_mutex_init(&m);
    int jr = wv_join(t, &rv);
    check("library still works: create + join", jr, WV_OK);
    check("library still works: return value 42", (int)(long)rv, 42);

    printf("  %d checks passed, %d failed\n", passed, failed);
    printf(failed == 0 ? "T8 PASS\n" : "T8 FAIL\n");
    return failed == 0 ? 0 : 1;
}
