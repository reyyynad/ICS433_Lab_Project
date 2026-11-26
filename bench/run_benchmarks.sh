#!/bin/sh
# Runs every benchmark, saves CSV files in bench/results/ and draws the graphs
# into docs/figures/ (needs python3 + matplotlib for the graphs).
# Usage: ./bench/run_benchmarks.sh        (from the project root, after make)
set -e
cd "$(dirname "$0")/.."
mkdir -p bench/results docs/figures
echo "[1/5] context switch and thread creation ..."; ./build/bench_switch --csv     > bench/results/switch.csv
echo "[2/5] synchronization primitives ...";        ./build/bench_sync --csv       > bench/results/sync.csv
echo "[3/5] switch cost vs number of threads ...";  ./build/bench_scale --threads  > bench/results/scale_threads.csv
echo "[4/5] time-quantum study ...";                ./build/bench_scale --quantum  > bench/results/quantum.csv
echo "[5/5] scheduling policy comparison ...";     ./build/bench_scale --policies > bench/results/policies.csv
{ echo "date: $(date -u '+%Y-%m-%d %H:%M UTC')"; uname -srm
  grep -m1 'model name' /proc/cpuinfo || true; echo "cpus: $(nproc)"
  gcc --version | head -1; } > bench/results/environment.txt
if python3 -c "import matplotlib" 2>/dev/null; then python3 bench/plot.py
else echo "matplotlib not installed: CSV files saved, graphs skipped (pip install matplotlib)"; fi
echo "Done. Results: bench/results/   Graphs: docs/figures/"
