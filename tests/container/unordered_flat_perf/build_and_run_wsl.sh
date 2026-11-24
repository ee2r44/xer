#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export PATH="$HOME/opt/cmake/bin:$HOME/opt:$PATH"
CXX="${CXX:-g++}"
PERF_SUFFIX="${PERF_SUFFIX:-gcc}"
BUILD="$SCRIPT_DIR/build-${PERF_SUFFIX}"
OUT="$ROOT/tests/container/_perf_out_unordered_flat_${PERF_SUFFIX}.txt"
EXE="$ROOT/tests/container/xer_unordered_flat_table_perf_${PERF_SUFFIX}"
mkdir -p "$BUILD"
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DPERF_SUFFIX="$PERF_SUFFIX" \
  -S "$SCRIPT_DIR" -B "$BUILD"
cmake --build "$BUILD" --config Release -j"$(nproc)"
"$EXE" | tee "$OUT"
