# THP Benchmarks Suite

This repository contains a set of benchmarks to experiment with **Transparent Huge Pages (THP)** on Linux.

## Benchmarks

1. **bench_thp_basic.c**  
   Measures read and write times for memory regions with and without THP enabled.

2. **bench_split_trigger.c**  
   Simulates partial freeing of a huge page (via `MADV_DONTNEED`) to observe kernel splitting behavior.

3. **bench_promotion.c**  
   Touches memory sparsely to see how Linux promotes base pages to huge pages over time.

## Automation Scripts

- **run_thp_benchmarks_perf_toggle.py**  
  - Runs all benchmarks multiple times.  
  - Collects wall-clock time, benchmark-reported time, `AnonHugePages` from `/proc/meminfo`, and `perf stat` counters (`cycles`, `page-faults`).  
  - Supports toggling THP mode (`always`/`never`) via `USE_SUDO` boolean.

- **log_thp_stats.sh**  
  - Logs `AnonHugePages` and `ShmemHugePages` over time.  
  - Useful for real-time observation of huge page allocation.

## Usage

### 1. Build benchmarks

```bash
cd benchmarks
make
````

### 2. Run benchmarks (with/without sudo for THP toggle)

```bash
# Toggle USE_SUDO in run_thp_benchmarks_perf_toggle.py as needed
sudo ./run_thp_benchmarks_perf_toggle.py
```

### 3. Outputs

* `results.csv` → contains wall_time, bench_time, AnonHugePages_kB, perf counters
* `results.png` → bar chart comparing wall times for each benchmark and THP mode

### Notes

* Make sure `perf` is installed to collect performance counters:

```bash
sudo apt install linux-tools-common linux-tools-$(uname -r)
```

* If running in a VM without permission to toggle THP, set `USE_SUDO = False`.
* `/proc/meminfo` is used to read anonymous huge page usage.
