#!/usr/bin/env python3
import subprocess
import os
import csv
import re
import time
import statistics
import matplotlib.pyplot as plt

# ---------- CONFIG ----------
BENCHES = [
    ("bench_thp_basic", ["20971520"]),      # 20 MB
    ("bench_split_trigger", []),
    ("bench_promotion", ["131072"]),        # stride = 128 KB
]
RUNS_PER_MODE = 5
RESULTS_CSV = "results.csv"
THP_PATH = "/sys/kernel/mm/transparent_hugepage/enabled"
MAKE_CMD = ["make"]
PERF_EVENTS = ["cycles", "page-faults"]
USE_SUDO = False   # <-- Toggle sudo usage for THP changes
# ----------------------------


def get_thp_status():
    try:
        with open(THP_PATH) as f:
            return f.read().strip()
    except PermissionError:
        return "Permission denied. Cannot read THP status."


def set_thp(mode: str):
    """mode ∈ {'always', 'never'}"""
    if USE_SUDO:
        print(f"[i] Setting THP mode → {mode} (with sudo)")
        subprocess.run(["sudo", "bash", "-c", f"echo {mode} > {THP_PATH}"], check=True)
        time.sleep(0.5)
    else:
        print(f"[!] Skipping THP mode change (sudo disabled). Current mode may remain as-is.")


def run_benchmark(binary, args):
    """Run benchmark normally"""
    cmd = ["./" + binary] + args
    print(f"[>] Running: {' '.join(cmd)}")
    start = time.time()
    try:
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, text=True)
    except subprocess.CalledProcessError as e:
        print(e.output)
        return None
    elapsed = time.time() - start
    match = re.search(r"Time:\s*([\d.]+)", output)
    reported = float(match.group(1)) if match else None
    anon_huge = read_meminfo("AnonHugePages")
    return {"wall_time": elapsed, "bench_time": reported, "AnonHugePages_kB": anon_huge}


def run_perf_benchmark(binary, args):
    """Run benchmark under perf stat"""
    cmd = ["perf", "stat", "-e", ",".join(PERF_EVENTS), "./" + binary] + args
    print(f"[>] Running perf: {' '.join(cmd)}")
    try:
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, text=True)
    except subprocess.CalledProcessError as e:
        output = e.output
    stats = {}
    for line in output.splitlines():
        for ev in PERF_EVENTS:
            if ev in line:
                m = re.match(r"\s*([\d,]+)\s+.*" + ev, line)
                if m:
                    stats[ev] = int(m.group(1).replace(",", ""))
    return stats


def read_meminfo(key):
    try:
        with open("/proc/meminfo") as f:
            for line in f:
                if line.startswith(key):
                    return int(line.split()[1])
    except Exception:
        return 0
    return 0


def build_benchmarks():
    print("[i] Building benchmarks…")
    subprocess.run(MAKE_CMD, check=True)


def record_results(rows):
    print(f"[i] Writing {RESULTS_CSV}")
    keys = rows[0].keys()
    with open(RESULTS_CSV, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=keys)
        writer.writeheader()
        writer.writerows(rows)


def plot_results(rows):
    plt.figure(figsize=(8, 5))
    grouped = {}
    for r in rows:
        key = (r["benchmark"], r["THP_mode"])
        grouped.setdefault(key, []).append(r["wall_time"])

    labels, values = [], []
    for (bench, mode), times in grouped.items():
        labels.append(f"{bench}\n({mode})")
        values.append(statistics.mean(times))

    plt.bar(labels, values)
    plt.ylabel("Average wall time (s)")
    plt.title("Benchmark comparison (THP on/off)")
    plt.tight_layout()
    plt.savefig("results.png")
    print("[i] Saved plot → results.png")


def main():
    build_benchmarks()
    all_results = []
    # If sudo is disabled, just use current THP mode
    thp_modes = ["always", "never"] if USE_SUDO else ["current"]

    for mode in thp_modes:
        set_thp(mode)
        thp_status = get_thp_status()
        print(f"[i] THP status:\n{thp_status}\n")

        for bench, args in BENCHES:
            for i in range(RUNS_PER_MODE):
                print(f"\n=== Run {i+1}/{RUNS_PER_MODE} | {bench} | THP={mode} ===")
                res = run_benchmark(bench, args)
                if res:
                    perf_stats = run_perf_benchmark(bench, args)
                    res.update(perf_stats)
                    res.update({"benchmark": bench, "THP_mode": mode, "run": i + 1})
                    all_results.append(res)

    record_results(all_results)
    plot_results(all_results)
    print("[✓] Done!")


if __name__ == "__main__":
    main()
