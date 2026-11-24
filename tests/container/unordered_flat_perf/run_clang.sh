#!/usr/bin/env bash
set -euo pipefail
export PATH="$HOME/opt/cmake/bin:$HOME/opt:$PATH"
rm -rf /mnt/d/develop/code/c++/xer_4/tests/container/unordered_flat_perf/build-clang
export CXX=clang++
export PERF_SUFFIX=clang
bash /mnt/d/develop/code/c++/xer_4/tests/container/unordered_flat_perf/build_and_run_wsl.sh
