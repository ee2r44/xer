from pathlib import Path

p = Path(__file__).with_name("extract_tools_wsl.sh")
p.write_text(
    """#!/usr/bin/env bash
set -euo pipefail
TOOLS=/mnt/d/develop/code/c++/xer_4/tests/container/unordered_flat_perf/tools
OPT="$HOME/opt"
mkdir -p "$OPT"
if [ ! -x "$OPT/cmake/bin/cmake" ]; then
  tar -xzf "$TOOLS/cmake-linux.tgz" -C "$OPT"
  rm -rf "$OPT/cmake"
  mv "$OPT"/cmake-3.31.6-linux-x86_64 "$OPT/cmake"
fi
if [ ! -x "$OPT/ninja" ]; then
  python3 - <<'PY'
import zipfile, os
zipfile.ZipFile("/mnt/d/develop/code/c++/xer_4/tests/container/unordered_flat_perf/tools/ninja-linux.zip").extractall(os.path.expanduser("~/opt"))
os.chmod(os.path.expanduser("~/opt/ninja"), 0o755)
PY
fi
export PATH="$OPT/cmake/bin:$OPT:$PATH"
cmake --version
ninja --version
""",
    newline="\n",
)
print("wrote", p)
