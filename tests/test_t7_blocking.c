/* ============================================================================
 * Test     : T7 - Blocking system call (sleep) stops all user threads
 * Owner    : Bana Jaber
 * Exit code: 0 = PASS, 1 = FAIL, 2 = NOT IMPLEMENTED YET
 * Expected : In the many-to-one model all user threads share one kernel
 *            thread. When one thread calls sleep(2), the kernel blocks that
 *            kernel thread, so the other user threads cannot run either: the
 *            heartbeat thread shows a gap of about 2 seconds although it was
 *            runnable the whole time.
 * Usage    : ./build/test_t7_blocking [--trace]
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../include/weave.h"

#define SLEEP_SEC  2

static volatile int sleeper_done = 0;   /* set when the sleeping thread ends */
static double max_gap = 0.0;            /* largest gap between heartbeats    */

/* Purpose : Read the monotonic clock.
 * Returns : Seconds as a double. */
static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* Purpose : Runnable thread that prints a heartbeat each time it runs and
 *           records the largest gap between two heartbeats. */
static void *heartbeat(void *arg)
{
    (void)arg;
    double start = now_sec(), last = start;
    int beat = 0;
    for (;;) {
        double t = now_sec();
        if (t - last > max_gap) max_gap = t - last;
        if (t - last > 0.5 || beat < 3)
            printf("  heartbeat %3d at %.3f s%s\n", beat, t - start,
                   t - last > 0.5 ? "   <-- all threads were stalled" : "");
        last = t;
        beat++;
        if (sleeper_done) break;        /* measured the gap after the sleep  */
        wv_yield();
    }
    printf("  heartbeat thread finished after %d beats\n", beat);
    return NULL;
}

/* Purpose : Thread that makes a blocking system call (sleep). */
static void *sleeper(void *arg)
{
    (void)arg;
    wv_yield();                         /* let the heartbeat start first     */
    wv_yield();
    printf("  sleeper: calling sleep(%d) (blocks the whole kernel thread)\n", SLEEP_SEC);
    sleep(SLEEP_SEC);
    printf("  sleeper: woke up\n");
    sleeper_done = 1;
    return NULL;
}

int main(int argc, char **argv)
{
    if (wv_init(WV_POLICY_RR, 10) != WV_OK) { printf("T7 FAIL: wv_init\n"); return 1; }
    if (argc > 1 && strcmp(argv[1], "--trace") == 0) wv_trace_enable(1);

    printf("T7 Blocking system call: one thread sleeps %d s, one thread is runnable\n",
           SLEEP_SEC);
    wv_tid_t h = wv_create(heartbeat, NULL, 1);
    wv_tid_t s = wv_create(sleeper,   NULL, 1);
    if (h < 0 || s < 0) { printf("T7 FAIL: wv_create\n"); return 1; }
    wv_join(s, NULL);
    wv_join(h, NULL);

    printf("  largest gap between heartbeats: %.3f s\n", max_gap);
    int ok = (max_gap >= SLEEP_SEC * 0.9);
    printf(ok ? "T7 PASS (many-to-one limitation shown)\n" : "T7 FAIL\n");
    return ok ? 0 : 1;
}
