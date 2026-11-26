/* ============================================================================
 * Test     : T10 - Condition variables, trylock, trywait and destroy
 * Owner    : Shatha Alharbi
 * Exit code: 0 = PASS, 1 = FAIL, 2 = NOT IMPLEMENTED YET
 * Expected :
 *   Part A  Monitor bounded buffer (1 mutex + 2 condition variables, size 4,
 *           4 producers, 4 consumers, 400 items, timer preemption on): every
 *           item is consumed exactly once and the buffer never overflows.
 *   Part B  wv_cond_signal wakes exactly ONE waiter, in FIFO order.
 *   Part C  wv_cond_broadcast wakes ALL waiters.
 *   Part D  Non-blocking calls and error codes: trylock, trywait, destroy,
 *           cond_wait without owning the mutex.
 * Usage    : ./build/test_t10_condvar [--verbose]
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../include/weave.h"

static int checks_ok = 0, checks_bad = 0;

/* Purpose : Print one check and count it. */
static void check(const char *what, int ok)
{
    printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what);
    if (ok) checks_ok++; else checks_bad++;
}

/* Purpose : Burn a little CPU so the timer can preempt inside critical sections. */
static void spin(int n) { for (volatile int i = 0; i < n; i++) ; }

/* ------------------------------------------------ Part A: monitor buffer --- */
#define BUF      4
#define NPROD    4
#define NCONS    4
#define PER_PROD 100
#define TOTAL    (NPROD * PER_PROD)

static int        buf[BUF], in, out, count, max_count;
static int        seen[TOTAL];
static wv_mutex_t bm;
static wv_cond_t  not_full, not_empty;

static void *producer(void *arg)
{
    int p = (int)(intptr_t)arg;
    for (int i = 0; i < PER_PROD; i++) {
        wv_mutex_lock(&bm);
        while (count == BUF) wv_cond_wait(&not_full, &bm);   /* Mesa: re-check */
        buf[in] = p * PER_PROD + i;
        spin(30000);                                         /* widen the window */
        in = (in + 1) % BUF;
        if (++count > max_count) max_count = count;
        wv_cond_signal(&not_empty);
        wv_mutex_unlock(&bm);
    }
    return NULL;
}

static void *consumer(void *arg)
{
    (void)arg;
    for (int i = 0; i < TOTAL / NCONS; i++) {
        wv_mutex_lock(&bm);
        while (count == 0) wv_cond_wait(&not_empty, &bm);
        int item = buf[out];
        spin(30000);
        out = (out + 1) % BUF;
        count--;
        wv_cond_signal(&not_full);
        wv_mutex_unlock(&bm);
        if (item >= 0 && item < TOTAL) seen[item]++;
    }
    return NULL;
}

static void part_a(void)
{
    printf("Part A: monitor bounded buffer (size %d, %d producers, %d consumers, %d items)\n",
           BUF, NPROD, NCONS, TOTAL);
    wv_mutex_init(&bm); wv_cond_init(&not_full); wv_cond_init(&not_empty);
    wv_tid_t t[NPROD + NCONS]; int n = 0;
    for (int c = 0; c < NCONS; c++) t[n++] = wv_create(consumer, NULL, 1);
    for (int p = 0; p < NPROD; p++) t[n++] = wv_create(producer, (void *)(intptr_t)p, 1);
    for (int i = 0; i < n; i++) wv_join(t[i], NULL);

    int once = 0;
    for (int i = 0; i < TOTAL; i++) once += (seen[i] == 1);
    printf("         items consumed exactly once: %d / %d, largest occupancy %d\n",
           once, TOTAL, max_count);
    check("every item consumed exactly once", once == TOTAL);
    check("buffer never held more than its size", max_count <= BUF && count == 0);
}

/* ------------------------------------------- Part B and C: signal/broadcast --- */
#define NWAIT 5
static wv_mutex_t gm;
static wv_cond_t  gc;
static int        tickets;               /* how many waiters may leave        */
static int        woke_order[NWAIT], woke_n, waiting_n;

static void *waiter(void *arg)
{
    int me = (int)(intptr_t)arg;
    wv_mutex_lock(&gm);
    waiting_n++;
    while (tickets == 0) wv_cond_wait(&gc, &gm);
    tickets--;
    woke_order[woke_n++] = me;
    wv_mutex_unlock(&gm);
    return NULL;
}

/* Purpose : Let other threads run until every waiter is blocked. */
static void settle(void) { for (int i = 0; i < 50; i++) wv_yield(); }

static void part_b_c(void)
{
    printf("Part B: wv_cond_signal wakes exactly one waiter, in FIFO order\n");
    wv_mutex_init(&gm); wv_cond_init(&gc);
    tickets = 0; woke_n = 0; waiting_n = 0;
    wv_tid_t t[NWAIT];
    for (int i = 0; i < NWAIT; i++) { t[i] = wv_create(waiter, (void *)(intptr_t)i, 1); settle(); }
    check("all 5 threads are waiting", waiting_n == NWAIT && woke_n == 0);
    check("destroy while threads wait returns WV_ERR_BUSY", wv_cond_destroy(&gc) == WV_ERR_BUSY);

    for (int k = 0; k < 2; k++) {        /* two signals, one ticket each      */
        wv_mutex_lock(&gm); tickets++; wv_cond_signal(&gc); wv_mutex_unlock(&gm);
        settle();
    }
    printf("         after 2 signals, %d threads woke (order: %d, %d)\n",
           woke_n, woke_order[0], woke_order[1]);
    check("each signal woke exactly one thread", woke_n == 2);
    check("woken in the order they waited (0 then 1)", woke_order[0] == 0 && woke_order[1] == 1);

    printf("Part C: wv_cond_broadcast wakes all remaining waiters\n");
    wv_mutex_lock(&gm); tickets += NWAIT - 2; wv_cond_broadcast(&gc); wv_mutex_unlock(&gm);
    for (int i = 0; i < NWAIT; i++) wv_join(t[i], NULL);
    printf("         after broadcast, %d / %d threads finished\n", woke_n, NWAIT);
    check("broadcast woke the other 3 threads", woke_n == NWAIT);
    check("signal with no waiters is harmless", wv_cond_signal(&gc) == WV_OK);
    check("destroy of an idle condition variable", wv_cond_destroy(&gc) == WV_OK);
}

/* ----------------------------------------------- Part D: non-blocking + errors */
static wv_mutex_t dm;
static void *try_other(void *arg) { (void)arg; return (void *)(intptr_t)wv_mutex_trylock(&dm); }

static void part_d(void)
{
    printf("Part D: non-blocking calls and error codes\n");
    wv_mutex_init(&dm);
    check("trylock on a free mutex -> WV_OK", wv_mutex_trylock(&dm) == WV_OK);
    check("trylock again by the owner -> WV_ERR_INVALID", wv_mutex_trylock(&dm) == WV_ERR_INVALID);
    void *rv; wv_tid_t t = wv_create(try_other, NULL, 1); wv_join(t, &rv);
    check("trylock by another thread -> WV_ERR_BUSY", (int)(intptr_t)rv == WV_ERR_BUSY);
    check("destroy of a locked mutex -> WV_ERR_BUSY", wv_mutex_destroy(&dm) == WV_ERR_BUSY);
    wv_mutex_unlock(&dm);
    check("destroy of a free mutex -> WV_OK", wv_mutex_destroy(&dm) == WV_OK);

    wv_sem_t s; wv_sem_init(&s, 1);
    check("trywait with value 1 -> WV_OK", wv_sem_trywait(&s) == WV_OK);
    check("trywait with value 0 -> WV_ERR_BUSY", wv_sem_trywait(&s) == WV_ERR_BUSY);
    check("destroy of an idle semaphore -> WV_OK", wv_sem_destroy(&s) == WV_OK);

    wv_cond_t c; wv_cond_init(&c); wv_mutex_init(&dm);
    check("cond_wait without owning the mutex -> WV_ERR_NOTOWNER",
          wv_cond_wait(&c, &dm) == WV_ERR_NOTOWNER);
    check("cond_wait(NULL, m) -> WV_ERR_INVALID", wv_cond_wait(NULL, &dm) == WV_ERR_INVALID);
    check("cond_init(NULL) -> WV_ERR_INVALID", wv_cond_init(NULL) == WV_ERR_INVALID);
    check("cond_signal(NULL) -> WV_ERR_INVALID", wv_cond_signal(NULL) == WV_ERR_INVALID);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (wv_init(WV_POLICY_RR, 1) != WV_OK) { printf("T10 FAIL: wv_init\n"); return 1; }
    printf("T10 Condition variables and non-blocking synchronization\n");
    part_a();
    part_b_c();
    part_d();
    printf("  %d checks passed, %d failed\n", checks_ok, checks_bad);
    printf(checks_bad == 0 ? "T10 PASS\n" : "T10 FAIL\n");
    return checks_bad == 0 ? 0 : 1;
}
