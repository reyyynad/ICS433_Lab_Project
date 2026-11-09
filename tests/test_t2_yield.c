/* ============================================================================
 * Test     : T2 - Cooperative yield (strict A B A B output)
 * Owner    : Renad Adel
 * Exit code: 0 = PASS, 1 = FAIL, 2 = NOT IMPLEMENTED YET
 * Expected : Two threads that alternately call wv_yield in a loop produce
 *            strictly interleaved output (see section 9 of the proposal).
 * Usage    : ./build/test_t2_yield [--trace]
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../include/weave.h"

#define ROUNDS 5

static char seq[2 * ROUNDS + 1];   /* the order in which the threads ran */
static int  pos = 0;

/* Each thread records its letter, prints it, and yields; ROUNDS times. */
static void *worker(void *arg)
{
    char letter = (char)(intptr_t)arg;
    for (int i = 0; i < ROUNDS; i++) {
        seq[pos++] = letter;
        printf("  thread %c: round %d\n", letter, i + 1);
        wv_yield();
    }
    return NULL;
}

int main(int argc, char **argv)
{
    if (wv_init(WV_POLICY_RR, 10) != WV_OK) { printf("T2 FAIL: wv_init\n"); return 1; }
    if (argc > 1 && strcmp(argv[1], "--trace") == 0) wv_trace_enable(1);

    printf("T2 Cooperative yield: two threads, %d rounds each\n", ROUNDS);
    wv_tid_t a = wv_create(worker, (void *)(intptr_t)'A', 1);
    wv_tid_t b = wv_create(worker, (void *)(intptr_t)'B', 1);
    wv_join(a, NULL);
    wv_join(b, NULL);

    seq[pos] = '\0';
    printf("  order: %s\n", seq);
    int ok = (pos == 2 * ROUNDS);
    for (int i = 0; ok && i < pos; i++)
        if (seq[i] != (i % 2 == 0 ? 'A' : 'B')) ok = 0;
    printf(ok ? "T2 PASS\n" : "T2 FAIL\n");
    return ok ? 0 : 1;
}
