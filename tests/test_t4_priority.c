/* ============================================================================
 * Test     : T4 - Priority scheduling with aging
 * Owner    : Nada Alrobaian
 * Purpose  : Verify that aging prevents a low-priority thread from starving.
 * ========================================================================== */

#include <stdio.h>
#include "../include/weave.h"

/* Set to 1 when the low-priority thread finally runs. */
static volatile int low_priority_ran = 0;

/*
 * Purpose : CPU-bound high-priority thread.
 * These threads do not voluntarily yield.
 */
static void *high_priority_worker(void *argument)
{
    volatile unsigned long counter = 0;

    (void)argument;

    while (1) {
        counter++;
    }

    return NULL;
}

/*
 * Purpose : Low-priority thread.
 * It should eventually run when aging is enabled.
 */
static void *low_priority_worker(void *argument)
{
    (void)argument;

    low_priority_ran = 1;
    printf("Low-priority thread eventually ran\n");

    wv_exit(NULL);
    return NULL;
}

int main(void)
{
    wv_tid_t high_one;
    wv_tid_t high_two;
    wv_tid_t high_three;
    wv_tid_t low_thread;

    printf("T4 Priority scheduling with aging\n");

    /*
     * Start Weave using priority scheduling.
     * The scheduler ages waiting threads after each scheduling decision.
     */
    if (wv_init(WV_POLICY_PRIORITY, 10) != WV_OK) {
        printf("T4 FAIL: wv_init failed\n");
        return 1;
    }

    /*
     * High-priority threads continuously use the CPU.
     */
    high_one = wv_create(high_priority_worker, NULL, 10);
    high_two = wv_create(high_priority_worker, NULL, 10);
    high_three = wv_create(high_priority_worker, NULL, 10);

    /*
     * This thread starts with a much lower priority.
     */
    low_thread = wv_create(low_priority_worker, NULL, 1);

    if (high_one < 0 ||
        high_two < 0 ||
        high_three < 0 ||
        low_thread < 0) {
        printf("T4 FAIL: thread creation failed\n");
        return 1;
    }

    /*
     * If aging is working, the low-priority thread's effective priority
     * eventually increases and it receives CPU time.
     */
    while (low_priority_ran == 0) {
        /*
         * Do not call wv_yield().
         * Timer preemption must allow the scheduler to continue.
         */
    }

    printf("T4 PASS: aging prevented low-priority starvation\n");

    /*
     * The high-priority threads are intentionally not joined because
     * they run continuously. The process ends after the test returns.
     */
    return 0;
}
