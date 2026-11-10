/* ============================================================================
 * Test     : T3 - Preemption under Round Robin
 * Owner    : Nada Alrobaian
 * Purpose  : Verify that timer preemption allows other threads to run even
 *            when one thread never calls wv_yield().
 * ========================================================================== */

#include <stdio.h>
#include "../include/weave.h"

/* Number of normal worker threads that must finish. */
static volatile int finished_workers = 0;

/*
 * Purpose : Simulate a CPU-bound thread.
 * This thread never calls wv_yield(). It depends on timer preemption.
 */
static void *infinite_worker(void *argument)
{
    volatile unsigned long counter = 0;

    (void)argument;

    while (1) {
        counter++;

        /*
         * The loop intentionally never calls wv_yield().
         * SIGVTALRM must interrupt this thread automatically.
         */
    }

    return NULL;
}

/* Purpose : Run a normal worker that eventually terminates. */
static void *normal_worker(void *argument)
{
    int i;

    (void)argument;

    for (i = 0; i < 100000; i++) {
        /*
         * CPU work. The timer should preempt the other infinite thread
         * and allow this worker to execute.
         */
    }
printf("A normal worker completed while the infinite-loop thread was running\n");
    finished_workers++;
    wv_exit(NULL);

    return NULL;
}

int main(void)
{
    wv_tid_t infinite_thread;
    wv_tid_t worker_one;
    wv_tid_t worker_two;
    wv_tid_t worker_three;

    printf("T3 Preemption under Round Robin\n");

    /*
     * Start Weave using Round Robin with a 10 ms time quantum.
     */
    if (wv_init(WV_POLICY_RR, 10) != WV_OK) {
        printf("T3 FAIL: wv_init failed\n");
        return 1;
    }
wv_trace_enable(1);
    /*
     * This thread never yields. If timer preemption is not working,
     * the three normal workers will never finish.
     */
    infinite_thread = wv_create(infinite_worker, NULL, 1);

    worker_one = wv_create(normal_worker, NULL, 1);
    worker_two = wv_create(normal_worker, NULL, 1);
    worker_three = wv_create(normal_worker, NULL, 1);

    if (infinite_thread < 0 ||
        worker_one < 0 ||
        worker_two < 0 ||
        worker_three < 0) {
        printf("T3 FAIL: thread creation failed\n");
        return 1;
    }

    /*
     * Wait until all normal workers have completed.
     * This loop also depends on timer preemption.
     */
    while (finished_workers < 3) {
        /* Do not call wv_yield(). The timer must preempt automatically. */
    }

    printf("T3 PASS: timer preemption allowed all workers to run\n");

    /*
     * The infinite thread is intentionally not joined because it never exits.
     * The process ends after the test returns.
     */
    return 0;
}
