#!/bin/sh
# ============================================================================
# Weave - guided live demonstration (final presentation)
# Runs the key programs one at a time and waits for Enter between steps.
# Usage: ./demo.sh            interactive (press Enter to go on)
#        ./demo.sh --no-pause  run every step without waiting
# ============================================================================
cd "$(dirname "$0")"
PAUSE=1; [ "$1" = "--no-pause" ] && PAUSE=0

step() {
  printf '\n\033[1;36m==== %s ====\033[0m\n' "$1"
  printf '\033[0;33m$ %s\033[0m\n' "$2"
  [ $PAUSE -eq 1 ] && { printf '(press Enter to run)'; read dummy; }
  sh -c "$2"
}

[ -x build/test_t1_lifecycle ] || make >/dev/null || exit 1

step "1. Thread lifecycle: create, run, exit, join (Renad)"      "./build/test_t1_lifecycle"
step "2. Cooperative switching with wv_yield (Renad)"            "./build/test_t2_yield"
step "3. Timer preemption: an infinite loop cannot keep the CPU (Nada)" "./build/test_t3_preempt"
step "4. Priority scheduling: starvation without aging, fixed by aging (Nada)" "./build/test_t4_priority"
step "5. Race condition WITHOUT a mutex (Shatha)"                "./build/test_t5_race --no-lock"
step "6. Same program WITH the mutex (Shatha)"                   "./build/test_t5_race --lock"
step "6b. Bank accounts: money lost without locks, conserved with them (Shatha)" "./build/app_bank"
step "7. Bounded buffer with semaphores (Shatha)"                "./build/test_t6_semaphore"
step "8. Condition variables: monitor buffer, signal, broadcast (Shatha)" "./build/test_t10_condvar"
step "9. Worker pool built from mutex + condvar + semaphore (Shatha)" "./build/app_workqueue"
step "10. Many-to-one limitation: a blocking sleep() stalls every thread (Bana)" "./build/test_t7_blocking"
step "11. Mixed workload: trace and per-thread statistics (Bana)" "./build/app_mixed --stats"
step "12. Stress: 500 threads, one mutex, 1 ms quantum (Bana)"   "./build/test_t9_stress"
step "13. Error handling: documented error codes (Shatha)"       "./build/test_t8_errors"
step "14. Full automated test suite (all)"                       "./tests/run_tests.sh"
step "15. Weave vs pthreads: switch and creation cost (Bana)"    "./build/bench_switch"
printf '\n\033[1;32mDemo finished.\033[0m\n'
