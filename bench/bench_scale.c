/* ============================================================================
 * Module   : Benchmark: scalability and time-quantum study (bench_scale.c)
 * Owner    : Shatha Alharbi, Bana Jaber
 * Purpose  : Two experiments, each configuration in its own child process
 *            (wv_init may be called once per process):
 *  1. --threads : cost of one wv_yield switch as the number of ready threads
 *                 grows (2 ... 1000), under Round Robin and under Priority.
 *                 RR picks the queue head in O(1); Priority scans the whole
 *                 ready queue and ages every waiting thread, O(n) per switch.
 *  3. --policies: 4 CPU-bound jobs of different lengths and priorities under
 *                 Round Robin, Priority + aging and Priority without aging.
 *                 Reports per-job response, turnaround (completion) and
 *                 waiting time = turnaround - job length measured alone.
 *  2. --quantum : 4 CPU-bound threads (each ~CPU_WORK_MS of work) plus one
 *                 interactive thread created last, for quanta 1..100 ms.
 *                 Reports total completion time, timer-driven switches, and
 *                 the response time of the interactive thread.
 * OS concepts : Scheduler complexity, time-quantum trade-off (overhead vs
 *               responsiveness), preemption
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * Usage    : ./build/bench_scale --threads | --quantum | --policies  (CSV on stdout)
 * ========================================================================== */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdint.h>
#include "../include/weave.h"
#include "../src/internal.h"     /* read the per-thread switch counter */

#define TOTAL_SWITCHES 200000
#define RUNS           3
#define NCPU           4

static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

/* ------------------------------------------------ 1. switch cost vs threads */
static int yields_per_thread;
static void *yielder(void *x) { (void)x; for (int i = 0; i < yields_per_thread; i++) wv_yield(); return NULL; }

static void child_threads(wv_policy_t pol, int n)
{
    wv_init(pol, 1000);                          /* long quantum: only wv_yield switches */
    static wv_tid_t t[WV_MAX_THREADS];
    double best = 1e18;
    for (int r = 0; r < RUNS; r++) {
        yields_per_thread = TOTAL_SWITCHES / n;
        for (int i = 0; i < n; i++) t[i] = wv_create(yielder, NULL, 1);
        double s = now_us();                     /* all created: time the switching */
        for (int i = 0; i < n; i++) wv_join(t[i], NULL);
        double per = (now_us() - s) / ((double)yields_per_thread * n);
        if (per < best) best = per;
    }
    printf("%s,%d,%.4f\n", pol == WV_POLICY_RR ? "rr" : "priority", n, best);
    fflush(stdout);
}

/* ------------------------------------------------ 2. time-quantum study ---- */
#define CPU_WORK_LOOPS 150000000L                /* ~0.1-0.3 s of CPU per thread */
static double t_start, t_inter_first;
static long   cpu_switches;

static void *cpu_bound(void *x)
{
    (void)x;
    for (volatile long i = 0; i < CPU_WORK_LOOPS; i++) ;
    cpu_switches += tcb_current()->switches;    /* how often it got the CPU */
    return NULL;
}

static void *interactive(void *x)
{
    (void)x;
    t_inter_first = now_us();                    /* first time it ran       */
    return NULL;
}

static void child_quantum(int q)
{
    wv_init(WV_POLICY_RR, q);
    wv_tid_t t[NCPU + 1];
    t_start = now_us();
    for (int i = 0; i < NCPU; i++) t[i] = wv_create(cpu_bound, NULL, 1);
    wv_yield();                                  /* let the CPU-bound threads start */
    double created = now_us();
    t[NCPU] = wv_create(interactive, NULL, 1);
    for (int i = 0; i <= NCPU; i++) wv_join(t[i], NULL);
    double total = now_us() - t_start;
    printf("%d,%.1f,%ld,%.1f\n", q, total / 1000.0, cpu_switches, (t_inter_first - created) / 1000.0);
    fflush(stdout);
}

/* ------------------------------------------------ 3. policy comparison ---- */
#define NJOBS     4
#define UNIT      40000000L                      /* loop iterations per work unit */
static const int job_units[NJOBS] = { 4, 3, 2, 1 };      /* job lengths       */
static const int job_prio[NJOBS]  = { 1, 2, 3, 4 };      /* longest = lowest  */
static double job_first[NJOBS], job_end[NJOBS], t0_pol;

static void *job(void *x)
{
    int j = (int)(intptr_t)x;
    job_first[j] = now_us();
    for (volatile long i = 0; i < job_units[j] * UNIT; i++) ;
    job_end[j] = now_us();
    return NULL;
}

static void child_policy(wv_policy_t pol, double unit_ms)
{
    const char *name = pol == WV_POLICY_RR ? "rr" : pol == WV_POLICY_PRIORITY ? "priority_aging" : "priority_no_aging";
    wv_init(pol, 10);
    wv_tid_t t[NJOBS];
    t0_pol = now_us();
    for (int j = 0; j < NJOBS; j++) t[j] = wv_create(job, (void *)(intptr_t)j, job_prio[j]);
    for (int j = 0; j < NJOBS; j++) wv_join(t[j], NULL);
    for (int j = 0; j < NJOBS; j++) {
        double service = job_units[j] * unit_ms;
        double resp = (job_first[j] - t0_pol) / 1000.0, turn = (job_end[j] - t0_pol) / 1000.0;
        printf("%s,%d,%d,%.1f,%.1f,%.1f,%.1f\n", name, j + 1, job_prio[j], service, resp, turn,
               turn - service > 0 ? turn - service : 0);
    }
    fflush(stdout);
}

/* ---------------------------------------------------------------- main ---- */
static void in_child(void (*fn)(void *), void *arg)
{
    pid_t pid = fork();
    if (pid == 0) { fn(arg); _exit(0); }
    waitpid(pid, NULL, 0);
}

struct targ { wv_policy_t p; int n; };
struct parg { wv_policy_t p; double unit_ms; };
static void run_p(void *a) { struct parg *p = a; child_policy(p->p, p->unit_ms); }
static void run_t(void *a) { struct targ *t = a; child_threads(t->p, t->n); }
static void run_q(void *a) { child_quantum(*(int *)a); }

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--threads") == 0) {
        int ns[] = { 2, 8, 32, 128, 512, 1000 };
        printf("policy,threads,switch_us\n"); fflush(stdout);
        for (int p = 0; p < 2; p++)
            for (unsigned i = 0; i < sizeof ns / sizeof ns[0]; i++) {
                struct targ a = { p ? WV_POLICY_PRIORITY : WV_POLICY_RR, ns[i] };
                in_child(run_t, &a);
            }
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--quantum") == 0) {
        int qs[] = { 1, 2, 5, 10, 20, 50, 100 };
        printf("quantum_ms,total_ms,cpu_thread_switches,interactive_response_ms\n"); fflush(stdout);
        for (unsigned i = 0; i < sizeof qs / sizeof qs[0]; i++) in_child(run_q, &qs[i]);
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--policies") == 0) {
        double best = 1e18;                      /* length of one unit, alone */
        for (int r = 0; r < 3; r++) {
            double s0 = now_us();
            for (volatile long i = 0; i < UNIT; i++) ;
            double d = (now_us() - s0) / 1000.0;
            if (d < best) best = d;
        }
        printf("policy,job,priority,service_ms,response_ms,turnaround_ms,waiting_ms\n"); fflush(stdout);
        wv_policy_t ps[] = { WV_POLICY_RR, WV_POLICY_PRIORITY, WV_POLICY_PRIORITY_NOAGING };
        for (int i = 0; i < 3; i++) { struct parg a = { ps[i], best }; in_child(run_p, &a); }
        return 0;
    }
    printf("usage: %s --threads | --quantum | --policies\n", argv[0]);
    return 1;
}
