#!/usr/bin/env bash
set -euo pipefail

SRC="bench_rand_access.c"
BIN="./bench_rand_access"
THP_COUNT=30
RUNTIME=20
INTERVAL=1
BATCH=10000
PAUSE_MS=200
ADVISE="--huge"   # or --nohuge or empty

echo "=== Simple THP benchmark runner ==="
echo "Benchmark source : $SRC"
echo "Binary           : $BIN"
echo "Args             : -n $THP_COUNT -t $RUNTIME -b $BATCH -p $PAUSE_MS $ADVISE"
echo "Sample interval  : ${INTERVAL}s"
echo

echo "[1/3] Building..."
if [[ ! -x "$BIN" ]]; then
  if make >/dev/null 2>&1; then
    true
  fi
fi
if [[ ! -x "$BIN" ]]; then
  echo "make didn't produce $BIN; compiling with gcc..."
  gcc -O2 -Wall -Wextra -o "$BIN" "$SRC"
fi
echo "Build OK"
echo

read_smaps_fields() {
  local pid="$1"
  awk '
    /^Rss:/ {rss=$2}
    /^Anonymous:/ {anon=$2}
    /^AnonHugePages:/ {anonhuge=$2}
    /^FilePmdMapped:/ {filepmd=$2}
    /^ShmemPmdMapped:/ {shmempmd=$2}
    END { printf "%d %d %d %d %d\n", rss, anon, anonhuge, filepmd, shmempmd }
  ' "/proc/$pid/smaps_rollup"
}

print_meminfo_excerpt() {
  echo "  System (/proc/meminfo):"
  grep -E 'MemFree:|MemAvailable:|AnonPages:|AnonHugePages:' /proc/meminfo \
    | sed 's/^/    /'
}

echo "[2/3] Starting benchmark..."
TMPLOG="$(mktemp)"
trap 'rm -f "$TMPLOG"' EXIT

# Start in background, capture stdout to a file
"$BIN" -n "$THP_COUNT" -t "$RUNTIME" -b "$BATCH" -p "$PAUSE_MS" $ADVISE >"$TMPLOG" 2>&1 &
BPID=$!

# Wait briefly for the program to print its pid line
PID=""
for _ in $(seq 1 50); do
  if [[ -s "$TMPLOG" ]]; then
    LINE="$(head -n 1 "$TMPLOG" || true)"
    PID="$(echo "$LINE" | sed -n 's/.*pid=\([0-9]\+\).*/\1/p')"
    if [[ -n "$PID" ]]; then
      echo "Benchmark line: $LINE"
      echo "Benchmark PID : $PID"
      echo
      break
    fi
  fi
  sleep 0.05
done

if [[ -z "$PID" ]]; then
  echo "ERROR: could not parse pid=... from benchmark output."
  echo "Benchmark output so far:"
  cat "$TMPLOG" || true
  kill "$BPID" 2>/dev/null || true
  exit 1
fi

echo "[3/3] Sampling (per-PID + system) until the benchmark exits..."
echo

while kill -0 "$BPID" 2>/dev/null && [[ -e "/proc/$PID/smaps_rollup" ]]; do
  TS="$(date +"%H:%M:%S.%3N")"

  read RSS_KB ANON_KB ANONHUGE_KB FILEPMD_KB SHMEMPMD_KB < <(read_smaps_fields "$PID")

  HUGE_KB=$((ANONHUGE_KB + FILEPMD_KB + SHMEMPMD_KB))
  NORMAL_KB=$((RSS_KB - HUGE_KB))
  [[ $NORMAL_KB -lt 0 ]] && NORMAL_KB=0

  echo "[$TS] PID $PID snapshot:"
  echo "  RSS total (in RAM):             ${RSS_KB} kB (~$((RSS_KB/1024)) MiB)"
  echo "  Huge-backed RSS (2MB pages):    ${HUGE_KB} kB (~$((HUGE_KB/1024)) MiB, ~$((${HUGE_KB}/2048)) huge pages)"
  echo "    - AnonHugePages (anon THP):   ${ANONHUGE_KB} kB (~$((${ANONHUGE_KB}/2048)) THP)"
  echo "    - FilePmdMapped (file 2MB):   ${FILEPMD_KB} kB"
  echo "    - ShmemPmdMapped (shmem 2MB): ${SHMEMPMD_KB} kB"
  echo "  Normal RSS remainder:           ${NORMAL_KB} kB (~$((NORMAL_KB/1024)) MiB)"
  echo "  Anonymous total:                ${ANON_KB} kB (~$((ANON_KB/1024)) MiB)"
  echo "  Anonymous normal (Anon-THP):    $((ANON_KB-ANONHUGE_KB)) kB"
  print_meminfo_excerpt
  echo

  sleep "$INTERVAL"
done

wait "$BPID" || true

echo "Benchmark process exited."
echo "Final benchmark output (first ~5 lines):"
head -n 5 "$TMPLOG" || true
echo
echo "Final system memory:"
print_meminfo_excerpt
echo
echo "=== Done ==="
