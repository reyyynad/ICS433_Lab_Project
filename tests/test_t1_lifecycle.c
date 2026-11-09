/* ============================================================================
 * Test     : T1 - Lifecycle (10 threads created, run, joined; no memory leaks)
 * Owner    : Renad Adel
 * Exit code: 0 = PASS, 1 = FAIL, 2 = NOT IMPLEMENTED YET
 * Expected : All threads run and terminate; join returns for each; Valgrind
 *            reports no leaks (see section 9 of the Phase 1 proposal).
 * Usage    : ./build/test_t1_lifecycle [--trace]
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../include/weave.h"

#define N 10

/* Each thread prints its number and ID and returns number * 10. */
static void *worker(void *arg)
{
    int n = (int)(intptr_t)arg;
    printf("  thread %2d running (tid %d)\n", n, wv_self());
    return (void *)(intptr_t)(n * 10);
}

int main(int argc, char **argv)
{
    int failures = 0;
    wv_tid_t tid[N];

    if (wv_init(WV_POLICY_RR, 10) != WV_OK) { printf("T1 FAIL: wv_init\n"); return 1; }
    if (argc > 1 && strcmp(argv[1], "--trace") == 0) wv_trace_enable(1);

    printf("T1 Lifecycle: creating %d threads\n", N);
    for (int i = 0; i < N; i++) {
        tid[i] = wv_create(worker, (void *)(intptr_t)i, 1);
        if (tid[i] < 0) { printf("T1 FAIL: wv_create returned %d\n", tid[i]); return 1; }
    }
    for (int i = 0; i < N; i++) {
        void *ret = NULL;
        if (wv_join(tid[i], &ret) != WV_OK || (int)(intptr_t)ret != i * 10) {
            printf("  FAIL: join of tid %d\n", tid[i]);
            failures++;
        } else {
            printf("  joined tid %2d, return value %d\n", tid[i], (int)(intptr_t)ret);
        }
    }
    if (wv_join(tid[0], NULL) != WV_ERR_INVALID) {     /* joining twice must fail */
        printf("  FAIL: second join should return an error\n");
        failures++;
    }
    printf(failures == 0 ? "T1 PASS\n" : "T1 FAIL\n");
    return failures ? 1 : 0;
}
