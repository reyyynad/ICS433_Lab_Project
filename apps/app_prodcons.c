/* ============================================================================
 * Module   : Example: bounded-buffer producer-consumer (app_prodcons.c)
 * Owner    : Bana Jaber
 * Purpose  : Demonstrates the semaphore with producers and consumers.
 *            Each line shows who produced or consumed which item and how full
 *            the buffer is. Consumers are slower than producers, so the buffer
 *            fills up and producers must block on the 'empty' semaphore.
 * OS concepts : Semaphores, bounded buffer, mutual exclusion
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * Usage    : ./build/app_prodcons [--trace] [--stats]
 *
 * Classic solution: empty = BUF_SIZE (free slots), full = 0 (filled slots),
 * and a mutex around the buffer itself.
 * ========================================================================== */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../include/weave.h"

#define QUANTUM_MS  10
#define BUF_SIZE    3
#define NPROD       2
#define NCONS       2
#define ITEMS       6            /* produced by each producer                 */

static int        buffer[BUF_SIZE];
static int        in = 0, out = 0, count = 0;
static wv_sem_t   empty, full;
static wv_mutex_t lock;

/* Purpose : Print the buffer as [##.] (filled and free slots). */
static void show(const char *who, const char *verb, int item)
{
    char bar[BUF_SIZE + 1];
    for (int i = 0; i < BUF_SIZE; i++) bar[i] = i < count ? '#' : '.';
    bar[BUF_SIZE] = '\0';
    printf("  %-3s %-8s item %3d   buffer [%s] %d/%d\n",
           who, verb, item, bar, count, BUF_SIZE);
}

/* Purpose : Producer p makes ITEMS items numbered p*100 + 1, p*100 + 2, ... */
static void *producer(void *arg)
{
    int p = (int)(intptr_t)arg;
    char name[8];
    snprintf(name, sizeof name, "P%d", p);
    for (int i = 1; i <= ITEMS; i++) {
        int item = p * 100 + i;
        wv_sem_wait(&empty);                 /* wait for a free slot          */
        wv_mutex_lock(&lock);
        buffer[in] = item;
        in = (in + 1) % BUF_SIZE;
        count++;
        show(name, "produced", item);
        wv_mutex_unlock(&lock);
        wv_sem_post(&full);                  /* one more filled slot          */
    }
    return NULL;
}

/* Purpose : Consumer c takes its share of the items. */
static void *consumer(void *arg)
{
    int c = (int)(intptr_t)arg;
    char name[8];
    snprintf(name, sizeof name, "C%d", c);
    for (int i = 0; i < NPROD * ITEMS / NCONS; i++) {
        wv_sem_wait(&full);                  /* wait for a filled slot        */
        wv_mutex_lock(&lock);
        int item = buffer[out];
        out = (out + 1) % BUF_SIZE;
        count--;
        show(name, "consumed", item);
        wv_mutex_unlock(&lock);
        wv_sem_post(&empty);                 /* one more free slot            */
        wv_yield();                          /* consumers are slower, so the  */
    }                                        /* buffer fills and producers block */
    return NULL;
}

int main(int argc, char **argv)
{
    int stats = 0;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--trace") == 0) wv_trace_enable(1);
        else if (strcmp(argv[i], "--stats") == 0) stats = 1;
    }
    if (wv_init(WV_POLICY_RR, QUANTUM_MS) != WV_OK) { printf("wv_init failed\n"); return 1; }
    wv_sem_init(&empty, BUF_SIZE);
    wv_sem_init(&full, 0);
    wv_mutex_init(&lock);

    printf("Producer-consumer: buffer of %d, %d producers x %d items, %d consumers\n",
           BUF_SIZE, NPROD, ITEMS, NCONS);

    wv_tid_t t[NPROD + NCONS];
    int n = 0;
    for (int c = 1; c <= NCONS; c++) t[n++] = wv_create(consumer, (void *)(intptr_t)c, 1);
    for (int p = 1; p <= NPROD; p++) t[n++] = wv_create(producer, (void *)(intptr_t)p, 1);
    for (int i = 0; i < n; i++) wv_join(t[i], NULL);

    printf("All %d items produced and consumed; buffer is empty (%d/%d).\n",
           NPROD * ITEMS, count, BUF_SIZE);
    if (stats) wv_stats_print();
    return 0;
}
