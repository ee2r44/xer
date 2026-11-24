#!/usr/bin/env bash
set -euo pipefail
export PATH="$HOME/opt/cmake/bin:$HOME/opt:/usr/bin:$PATH"
ROOT=/mnt/d/develop/code/c++/xer_4
LABEL="${1:?label yes|no}"
SUFFIX="${2:?suffix gcc|clang}"
PERF_DIR="$ROOT/tests/container/unordered_flat_perf"
BUILD="$PERF_DIR/build-${SUFFIX}"
if [[ "$LABEL" == "yes" ]]; then
  INC="$ROOT/include"
elif [[ "$LABEL" == "no" ]]; then
  INC="$ROOT/tests/container/ab_mirror/find_noprefetch/include"
else
  echo "label must be yes|no" >&2
  exit 1
fi
echo "=== ${SUFFIX} pf_${LABEL} INC=${INC} ==="
cmake -DXER_INCLUDE_DIR="$INC" -DPERF_SUFFIX="$SUFFIX" -S "$PERF_DIR" -B "$BUILD"
find "$BUILD/CMakeFiles/xer_unordered_flat_table_perf.dir" -name '*.o' -delete 2>/dev/null || true
touch "$ROOT/tests/container/xer_unordered_flat_table_perf.cpp"
cmake --build "$BUILD" --config Release -j"$(nproc)"
OUT="$ROOT/tests/container/_perf_out_unordered_flat_${SUFFIX}_pf_ab_${LABEL}.txt"
"$ROOT/tests/container/xer_unordered_flat_table_perf_${SUFFIX}" > "$OUT"
wc -l "$OUT"
head -4 "$OUT"
