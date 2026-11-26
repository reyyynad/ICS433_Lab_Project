#!/bin/sh
# Runs every test and example application under Valgrind (memcheck).
# A program passes when Valgrind reports 0 errors and no leaked memory.
# Usage: ./tests/run_valgrind.sh   (from the project root, after make)
command -v valgrind >/dev/null || { echo "valgrind is not installed (sudo apt install valgrind)"; exit 1; }
pass=0; fail=0
for p in $(ls build/test_t* | sort -V) build/app_prodcons build/app_mixed build/app_workqueue build/app_bank; do
  valgrind -q --leak-check=full --errors-for-leak-kinds=definite,indirect \
           --error-exitcode=99 --trace-children=yes "$p" >/dev/null 2>/tmp/weave_vg.txt
  rc=$?
  if [ $rc -eq 99 ]; then echo "ERRORS $(basename $p)  (see /tmp/weave_vg.txt)"; fail=$((fail+1))
  elif [ $rc -ne 0 ]; then echo "FAIL   $(basename $p)  (program exit code $rc)"; fail=$((fail+1))
  else echo "CLEAN  $(basename $p)"; pass=$((pass+1)); fi
done
echo "Valgrind: $pass clean, $fail with problems"
[ "$fail" -eq 0 ]
