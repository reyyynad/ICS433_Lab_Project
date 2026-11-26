/* ============================================================================
 * Module   : Example: bank-account race condition (app_bank.c)
 * Owner    : Shatha Alharbi
 * Purpose  : Second race-condition demonstration promised in the proposal.
 *            5 accounts start with 1,000 SAR each (total 5,000). 8 teller
 *            threads make 20,000 random transfers each between two accounts.
 *            A transfer is read balance -> (preemption point) -> write balance.
 *            Without locking, concurrent transfers overwrite each other and
 *            money is created or destroyed: the total is no longer 5,000.
 *            Overdrafts are allowed, so only the race can change the total.
 *            With one mutex per account (always locked in increasing account
 *            order, so two transfers can never deadlock), the total is exact.
 * OS concepts : Race conditions, critical sections, fine-grained locking,
 *               deadlock avoidance by lock ordering
 * Project  : Weave - User-Level Thread Library (OS Lab Project, Group #1, F62)
 * Usage    : ./build/app_bank            (both runs; exit 0 if the locked run is exact)
 *            ./build/app_bank --no-lock | --lock
 * ========================================================================== */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../include/weave.h"

#define ACCOUNTS   5
#define START      1000
#define TELLERS    8
#define TRANSFERS  20000

static volatile long balance[ACCOUNTS];
static wv_mutex_t    lock[ACCOUNTS];
static int           use_lock;

/* Purpose : Small per-thread pseudo-random generator (rand() is shared state). */
static unsigned next_rand(unsigned *s) { *s = *s * 1103515245u + 12345u; return (*s >> 16) & 0x7fff; }

/* Purpose : One teller: random transfers of 1-50 SAR between two accounts. */
static void *teller(void *arg)
{
    unsigned seed = (unsigned)(intptr_t)arg * 7919u + 1u;
    for (int i = 0; i < TRANSFERS; i++) {
        int from = next_rand(&seed) % ACCOUNTS;
        int to   = next_rand(&seed) % ACCOUNTS;
        if (from == to) continue;
        long amount = 1 + next_rand(&seed) % 50;
        int first = from < to ? from : to, second = from < to ? to : from;

        if (use_lock) { wv_mutex_lock(&lock[first]); wv_mutex_lock(&lock[second]); }
        long a = balance[from], b = balance[to];            /* read          */
        if (i % 500 == 0) wv_yield();                       /* lose the CPU  */
        for (volatile int k = 0; k < 50; k++) ;             /* timer may fire */
        balance[from] = a - amount;                         /* write (may    */
        balance[to]   = b + amount;                         /* overwrite)    */
        if (use_lock) { wv_mutex_unlock(&lock[second]); wv_mutex_unlock(&lock[first]); }
    }
    return NULL;
}

/* Purpose : Run all tellers once and return the total money in the bank. */
static long run(int with_lock)
{
    use_lock = with_lock;
    for (int i = 0; i < ACCOUNTS; i++) { balance[i] = START; wv_mutex_init(&lock[i]); }
    wv_tid_t t[TELLERS];
    for (int i = 0; i < TELLERS; i++) t[i] = wv_create(teller, (void *)(intptr_t)i, 1);
    for (int i = 0; i < TELLERS; i++) wv_join(t[i], NULL);
    long total = 0;
    printf("  %-14s balances:", with_lock ? "WITH mutexes" : "WITHOUT mutex");
    for (int i = 0; i < ACCOUNTS; i++) { printf(" %6ld", balance[i]); total += balance[i]; }
    printf("   total = %ld (expected %d, difference %+ld)\n", total, ACCOUNTS * START,
           total - ACCOUNTS * START);
    return total;
}

int main(int argc, char **argv)
{
    int do_nolock = 1, do_lock = 1;
    if (argc > 1 && strcmp(argv[1], "--no-lock") == 0) do_lock = 0;
    if (argc > 1 && strcmp(argv[1], "--lock") == 0)    do_nolock = 0;
    if (wv_init(WV_POLICY_RR, 1) != WV_OK) { printf("wv_init failed\n"); return 1; }

    printf("Bank: %d accounts x %d SAR, %d tellers x %d transfers\n",
           ACCOUNTS, START, TELLERS, TRANSFERS);
    int ok = 1;
    if (do_nolock) {
        long t = run(0);
        printf(t != ACCOUNTS * START ? "  -> race condition: money was created or lost\n"
                                     : "  -> race not triggered this time\n");
    }
    if (do_lock) {
        long t = run(1);
        ok = (t == ACCOUNTS * START);
        printf(ok ? "  -> correct: total money is conserved\n" : "  -> ERROR: total changed\n");
    }
    return ok ? 0 : 1;
}
