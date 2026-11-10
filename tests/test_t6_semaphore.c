/* ============================================================================
 * Test     : T6 - Bounded-buffer producer-consumer with semaphores
 * Owner    : Shatha Alharbi
 * Exit code: 0 = PASS, 1 = FAIL, 2 = NOT IMPLEMENTED YET
 * Expected : Buffer of size 5, 3 producers and 3 consumers. Every item that is
 *            produced is consumed exactly once, the buffer never holds more
 *            than 5 items, and no thread deadlocks.
 * Usage    : ./build/test_t6_semaphore [--trace] [--verbose]
 *
 * Classic solution with three semaphores:
 *   empty = 5  (free slots)    full = 0  (filled slots)    mutex = 1 (buffer access)
 * ========================================================================== */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../include/weave.h"

#define BUF_SIZE        5
#define NPRODUCERS      3
#define NCONSUMERS      3
#define ITEMS_PER_PROD  20
#define TOTAL_ITEMS     (NPRODUCERS * ITEMS_PER_PROD)
#define ITEMS_PER_CONS  (TOTAL_ITEMS / NCONSUMERS)

static int      buffer[BUF_SIZE];
static int      in = 0, out = 0, count = 0, max_count = 0;
static int      consumed[TOTAL_ITEMS];     /* how many times each item was taken */
static int      per_consumer[NCONSUMERS];
static int      verbose = 0;
static wv_sem_t empty, full, mutex;

/* Purpose : Producer p creates items p*ITEMS_PER_PROD ... and puts them in the buffer. */
static void *producer(void *arg)
{
    int p = (int)(intptr_t)arg;
    for (int i = 0; i < ITEMS_PER_PROD; i++) {
        int item = p * ITEMS_PER_PROD + i;
        wv_sem_wait(&empty);                    /* wait for a free slot        */
        wv_sem_wait(&mutex);
        buffer[in] = item;
        in = (in + 1) % BUF_SIZE;
        if (++count > max_count) max_count = count;
        if (verbose) printf("  P%d put  item %2d  (buffer %d/%d)\n", p, item, count, BUF_SIZE);
        wv_sem_post(&mutex);
        wv_sem_post(&full);                     /* one more filled slot        */
        if (i % 3 == 0) wv_yield();             /* mix the order of the threads */
    }
    return NULL;
}

/* Purpose : Consumer c removes ITEMS_PER_CONS items and records each one. */
static void *consumer(void *arg)
{
    int c = (int)(intptr_t)arg;
    for (int i = 0; i < ITEMS_PER_CONS; i++) {
        wv_sem_wait(&full);                     /* wait for a filled slot      */
        wv_sem_wait(&mutex);
        int item = buffer[out];
        out = (out + 1) % BUF_SIZE;
        count--;
        if (verbose) printf("  C%d took item %2d  (buffer %d/%d)\n", c, item, count, BUF_SIZE);
        wv_sem_post(&mutex);
        wv_sem_post(&empty);                    /* one more free slot          */
        if (item >= 0 && item < TOTAL_ITEMS) consumed[item]++;
        per_consumer[c]++;
        if (i % 2 == 0) wv_yield();
    }
    return NULL;
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) wv_trace_enable(1);
        if (strcmp(argv[i], "--verbose") == 0) verbose = 1;
    }
    if (wv_init(WV_POLICY_RR, 10) != WV_OK) { printf("T6 FAIL: wv_init\n"); return 1; }
    if (wv_sem_init(&empty, BUF_SIZE) || wv_sem_init(&full, 0) || wv_sem_init(&mutex, 1)) {
        printf("T6 FAIL: wv_sem_init\n"); return 1;
    }

    printf("T6 Bounded buffer: size %d, %d producers, %d consumers, %d items\n",
           BUF_SIZE, NPRODUCERS, NCONSUMERS, TOTAL_ITEMS);

    wv_tid_t t[NPRODUCERS + NCONSUMERS];
    int n = 0;
    for (int c = 0; c < NCONSUMERS; c++) t[n++] = wv_create(consumer, (void *)(intptr_t)c, 1);
    for (int p = 0; p < NPRODUCERS; p++) t[n++] = wv_create(producer, (void *)(intptr_t)p, 1);
    for (int i = 0; i < n; i++) wv_join(t[i], NULL);

    int once = 0, missing = 0, dup = 0;
    for (int i = 0; i < TOTAL_ITEMS; i++) {
        if (consumed[i] == 1) once++;
        else if (consumed[i] == 0) missing++;
        else dup++;
    }
    for (int c = 0; c < NCONSUMERS; c++)
        printf("  consumer C%d took %d items\n", c, per_consumer[c]);
    printf("  items consumed exactly once: %d / %d (missing %d, duplicated %d)\n",
           once, TOTAL_ITEMS, missing, dup);
    printf("  largest buffer occupancy: %d (limit %d), items left in buffer: %d\n",
           max_count, BUF_SIZE, count);

    int ok = (once == TOTAL_ITEMS && max_count <= BUF_SIZE && count == 0);
    printf(ok ? "T6 PASS\n" : "T6 FAIL\n");
    return ok ? 0 : 1;
}
