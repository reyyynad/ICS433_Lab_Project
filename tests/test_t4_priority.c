/* ============================================================================
 * Test     : T4 - Priority scheduling with and without aging
 * Owner    : Nada Alrobaian (final phase: no-aging case added)
 * Exit code: 0 = PASS, 1 = FAIL
 * Expected : Three CPU-bound threads of priority 10 never yield; one thread of
 *            priority 1 only needs to run once.
 *              Case 1, WITHOUT aging: the low-priority thread never runs
 *                      during a 1-second window (starvation).
 *              Case 2, WITH aging   : the low-priority thread runs after a
 *                      short, bounded wait.
 *            Each case runs in its own child process (wv_init once per
 *            process). The main thread has priority 0, so without aging it
 *            cannot run either; a real-time alarm (SIGALRM, not used by
 *            Weave) ends the observation window.
 * Usage    : ./build/test_t4_priority
 * ========================================================================== */

#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../include/weave.h"

#define WINDOW_SEC 1

static volatile sig_atomic_t low_priority_ran = 0;
static double created_at, ran_at;

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

/* Purpose : CPU-bound high-priority thread; never yields. */
static void *high_priority_worker(void *argument)
{
    (void)argument;
    volatile unsigned long counter = 0;
    while (1) counter++;
    return NULL;
}

/* Purpose : Low-priority thread; records when it first gets the CPU. */
static void *low_priority_worker(void *argument)
{
    (void)argument;
    ran_at = now_ms();
    low_priority_ran = 1;
    return NULL;
}

/* Purpose : End of the observation window (only used without aging). */
static void window_over(int sig)
{
    (void)sig;
    /* Starvation shown if the low-priority thread never ran. */
    _exit(low_priority_ran ? 1 : 0);
}

/* Purpose : Run one case in a child process. Returns 0 on success. */
static int run_case(wv_policy_t policy)
{
    if (wv_init(policy, 10) != WV_OK) return 1;
    if (policy == WV_POLICY_PRIORITY_NOAGING) { signal(SIGALRM, window_over); alarm(WINDOW_SEC); }
    else                                      { alarm(10); }   /* safety timeout */

    for (int i = 0; i < 3; i++)
        if (wv_create(high_priority_worker, NULL, 10) < 0) return 1;
    created_at = now_ms();
    if (wv_create(low_priority_worker, NULL, 1) < 0) return 1;

    while (!low_priority_ran) ;          /* no wv_yield: only the timer switches */
    printf("  WITH aging   : low-priority thread ran after %.0f ms -> no starvation\n",
           ran_at - created_at);
    fflush(stdout);
    return 0;
}

int main(void)
{
    printf("T4 Priority scheduling: 3 threads of priority 10 (never yield) + 1 of priority 1\n");
    fflush(stdout);

    pid_t pid = fork();
    if (pid == 0) _exit(run_case(WV_POLICY_PRIORITY_NOAGING));
    int st1 = 0; waitpid(pid, &st1, 0);
    int starved = WIFEXITED(st1) && WEXITSTATUS(st1) == 0;
    printf("  WITHOUT aging: low-priority thread %s in %d s -> %s\n",
           starved ? "never ran" : "ran", WINDOW_SEC,
           starved ? "starvation shown" : "starvation NOT shown");
    fflush(stdout);

    pid = fork();
    if (pid == 0) _exit(run_case(WV_POLICY_PRIORITY));
    int st2 = 0; waitpid(pid, &st2, 0);
    int aged = WIFEXITED(st2) && WEXITSTATUS(st2) == 0;

    int ok = starved && aged;
    printf(ok ? "T4 PASS: aging prevented low-priority starvation\n" : "T4 FAIL\n");
    return ok ? 0 : 1;
}
