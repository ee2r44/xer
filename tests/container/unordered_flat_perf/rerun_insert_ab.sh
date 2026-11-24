#!/usr/bin/env bash
set -euo pipefail
export PATH="$HOME/opt/cmake/bin:$HOME/opt:/usr/bin:$PATH"
ROOT=/mnt/d/develop/code/c++/xer_4
LABEL="${1:?label}"
SUFFIX="${2:?suffix}"
BUILD="$ROOT/tests/container/unordered_flat_perf/build-${SUFFIX}"
echo "=== rebuild ${SUFFIX} label=${LABEL} ==="
find "$BUILD/CMakeFiles/xer_unordered_flat_table_perf.dir" -name '*.o' -delete 2>/dev/null || true
cmake --build "$BUILD" --config Release -j"$(nproc)"
OUT="$ROOT/tests/container/_perf_out_unordered_flat_${SUFFIX}_insert_ab_${LABEL}.txt"
echo "=== run ${SUFFIX} -> ${OUT} ==="
"$ROOT/tests/container/xer_unordered_flat_table_perf_${SUFFIX}" > "$OUT"
wc -l "$OUT"
head -4 "$OUT"
