#!/usr/bin/env bash
set -euo pipefail
export PATH="$HOME/opt/cmake/bin:$HOME/opt:$PATH"
SUFFIX="$1"
CXX="$2"
ROOT=/mnt/d/develop/code/c++/xer_4
BUILD="$ROOT/tests/container/unordered_flat_perf/build-${SUFFIX}"
find "$BUILD/CMakeFiles/xer_unordered_flat_table_perf.dir" -name '*.o' -delete 2>/dev/null || true
cmake --build "$BUILD" --config Release -j"$(nproc)"
"$ROOT/tests/container/xer_unordered_flat_table_perf_${SUFFIX}" \
  > "$ROOT/tests/container/_perf_out_unordered_flat_${SUFFIX}.txt"
