"""Owner: Bana Jaber - draws the benchmark graphs used in the final report.

Reads the CSV files written by bench/run_benchmarks.sh (bench/results/) and
saves PNG figures into docs/figures/.

Usage: python3 bench/plot.py      (from the project root)
"""
import csv
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES = os.path.join(ROOT, "bench", "results")
OUT = os.path.join(ROOT, "docs", "figures")
os.makedirs(OUT, exist_ok=True)

WEAVE, PTHREADS, PRIO = "#257de0", "#f47230", "#00b894"   # fixed series colours
TEXT, MUTED, GRID = "#191919", "#666666", "#e6e6e6"

plt.rcParams.update({
    "font.family": "DejaVu Sans", "font.size": 10, "axes.edgecolor": MUTED,
    "axes.labelcolor": TEXT, "xtick.color": MUTED, "ytick.color": MUTED,
    "axes.spines.top": False, "axes.spines.right": False, "axes.titlesize": 11,
    "axes.titleweight": "bold", "axes.titlecolor": TEXT, "legend.frameon": False,
})


def read(name):
    with open(os.path.join(RES, name)) as f:
        return list(csv.DictReader(f))


def fmt(v):
    return f"{v:.3f}" if v < 1 else (f"{v:.2f}" if v < 10 else f"{v:.1f}")


def fig_compare():
    """Figure: Weave vs pthreads, every measured operation (log scale)."""
    labels = {
        "create_join": "Create + join a thread",
        "switch": "Context switch (yield / hand-off)",
        "mutex_uncontended": "Mutex lock+unlock, no contention",
        "sem_handoff": "Semaphore hand-off, 2 threads",
        "cond_handoff": "Condition-variable hand-off, 2 threads",
        "mutex_contended": "Mutex, 4 threads contending",
    }
    rows = read("switch.csv") + read("sync.csv")
    rows = [r for r in rows if r["metric"] in labels][::-1]
    names = [labels[r["metric"]] for r in rows]
    w = [float(r["weave_us"]) for r in rows]
    p = [float(r["pthreads_us"]) for r in rows]
    y = range(len(rows))
    h = 0.36
    fig, ax = plt.subplots(figsize=(8.2, 4.6))
    ax.barh([i + h / 2 + 0.02 for i in y], w, h, color=WEAVE, label="Weave (user-level)")
    ax.barh([i - h / 2 - 0.02 for i in y], p, h, color=PTHREADS, label="pthreads (kernel-level)")
    for i in y:
        ax.text(w[i] * 1.12, i + h / 2 + 0.02, fmt(w[i]), va="center", fontsize=8, color=TEXT)
        ax.text(p[i] * 1.12, i - h / 2 - 0.02, fmt(p[i]), va="center", fontsize=8, color=TEXT)
    ax.set_yticks(list(y))
    ax.set_yticklabels(names)
    ax.set_xscale("log")
    ax.set_xlim(min(w + p) / 2, max(w + p) * 6)
    ax.set_xlabel("Time per operation (microseconds, log scale) - lower is better")
    ax.grid(axis="x", color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    ax.legend(loc="lower center", bbox_to_anchor=(0.5, 1.0), ncol=2)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "fig_weave_vs_pthreads.png"), dpi=200)
    plt.close(fig)


def fig_scale():
    """Figure: cost of one switch as the number of ready threads grows."""
    rows = read("scale_threads.csv")
    fig, ax = plt.subplots(figsize=(7.2, 4.0))
    for pol, col, name in (("rr", WEAVE, "Round Robin (O(1) pick)"),
                           ("priority", PRIO, "Priority + aging (O(n) scan)")):
        pts = [(int(r["threads"]), float(r["switch_us"])) for r in rows if r["policy"] == pol]
        xs, ys = zip(*pts)
        ax.plot(xs, ys, color=col, linewidth=2, marker="o", markersize=6, label=name)
        ax.annotate(f"{ys[-1]:.2f} us", (xs[-1], ys[-1]), textcoords="offset points",
                    xytext=(-8, 8), ha="right", fontsize=8, color=TEXT)
    ax.set_xscale("log")
    ax.set_xticks([2, 8, 32, 128, 512, 1000])
    ax.set_xticklabels(["2", "8", "32", "128", "512", "1000"])
    ax.set_xlabel("Number of ready threads (log scale)")
    ax.set_ylabel("Time per context switch (microseconds)")
    ax.grid(color=GRID, linewidth=0.8)
    ax.legend(loc="upper left")
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "fig_switch_vs_threads.png"), dpi=200)
    plt.close(fig)


def fig_quantum():
    """Figure: time-quantum trade-off, two panels sharing the x axis."""
    rows = read("quantum.csv")
    q = [int(r["quantum_ms"]) for r in rows]
    resp = [float(r["interactive_response_ms"]) for r in rows]
    sw = [int(r["cpu_thread_switches"]) for r in rows]
    tot = [float(r["total_ms"]) for r in rows]
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(9.0, 3.6))
    a1.plot(q, resp, color=WEAVE, linewidth=2, marker="o", markersize=6)
    a1.set_title("Response time of a newly created thread")
    a1.set_ylabel("Response time (ms)")
    a2.plot(q, sw, color=WEAVE, linewidth=2, marker="o", markersize=6)
    a2.set_title("Times the 4 CPU-bound threads got the CPU")
    a2.set_ylabel("Context switches (count)")
    for ax in (a1, a2):
        ax.set_xscale("log")
        ax.set_xticks(q)
        ax.set_xticklabels([str(v) for v in q])
        ax.set_xlabel("Time quantum (ms, log scale)")
        ax.grid(color=GRID, linewidth=0.8)
        ax.title.set_fontsize(10)
        ax.set_ylim(0, ax.get_ylim()[1] * 1.12)
    for x, y in zip(q, resp):
        a1.annotate(f"{y:.0f}", (x, y), textcoords="offset points", xytext=(0, 7), ha="center", fontsize=8)
    for x, y in zip(q, sw):
        a2.annotate(f"{y}", (x, y), textcoords="offset points", xytext=(0, 7), ha="center", fontsize=8)
    fig.text(0.5, 0.005, f"Total completion time stayed between {min(tot):.0f} and {max(tot):.0f} ms "
             "for every quantum.", ha="center", fontsize=8.5, color=MUTED)
    fig.tight_layout(rect=(0, 0.04, 1, 1))
    fig.savefig(os.path.join(OUT, "fig_quantum.png"), dpi=200)
    plt.close(fig)


def fig_policies():
    """Figure: waiting time of each job under the three priority settings."""
    rows = read("policies.csv")
    pols = [("rr", WEAVE, "Round Robin"), ("priority_aging", PRIO, "Priority + aging"),
            ("priority_no_aging", PTHREADS, "Priority, no aging")]
    jobs = sorted({int(r["job"]) for r in rows})
    fig, ax = plt.subplots(figsize=(7.6, 4.0))
    bw = 0.26
    for k, (pol, col, name) in enumerate(pols):
        vals = [float(next(r for r in rows if r["policy"] == pol and int(r["job"]) == j)["waiting_ms"]) for j in jobs]
        xs = [j + (k - 1) * (bw + 0.02) for j in jobs]
        ax.bar(xs, vals, bw, color=col, label=name)
        for x, v in zip(xs, vals):
            ax.text(x, v + 8, f"{v:.0f}", ha="center", fontsize=7.5, color=TEXT)
    labels = []
    for j in jobs:
        r = next(r for r in rows if int(r["job"]) == j)
        labels.append(f"Job {j}\n{float(r['service_ms']):.0f} ms, priority {r['priority']}")
    ax.set_xticks(jobs)
    ax.set_xticklabels(labels)
    ax.set_ylabel("Waiting time (ms)")
    ax.grid(axis="y", color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    ax.set_ylim(0, ax.get_ylim()[1] * 1.1)
    ax.legend(loc="upper right")
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "fig_policies.png"), dpi=200)
    plt.close(fig)


if __name__ == "__main__":
    fig_policies()
    fig_compare()
    fig_scale()
    fig_quantum()
    print("Graphs saved in", OUT)
