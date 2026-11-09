#!/bin/sh
# Runs every test program in build/ and prints PASS / FAIL / TODO for each.
pass=0; fail=0; todo=0
for t in build/test_t*; do
  [ -x "$t" ] || continue
  "$t" >/dev/null 2>&1; rc=$?
  name=$(basename "$t")
  case $rc in
    0) echo "PASS  $name"; pass=$((pass+1));;
    2) echo "TODO  $name"; todo=$((todo+1));;
    *) echo "FAIL  $name"; fail=$((fail+1));;
  esac
done
echo "Total: $pass passed, $fail failed, $todo not implemented"
[ "$fail" -eq 0 ]
