#!/usr/bin/env bash
set -euo pipefail
mkdir -p "$HOME/opt"
cd /tmp
if [ ! -x "$HOME/opt/cmake/bin/cmake" ]; then
  curl -fsSL -o cmake.tgz \
    https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-linux-x86_64.tar.gz
  tar -xzf cmake.tgz -C "$HOME/opt"
  rm -rf "$HOME/opt/cmake"
  mv "$HOME/opt"/cmake-3.31.6-linux-x86_64 "$HOME/opt/cmake"
fi
if [ ! -x "$HOME/opt/ninja" ]; then
  curl -fsSL -o ninja.zip \
    https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-linux.zip
  python3 - <<'PY'
import zipfile, os
zipfile.ZipFile("ninja.zip").extractall(os.path.expanduser("~/opt"))
os.chmod(os.path.expanduser("~/opt/ninja"), 0o755)
PY
fi
"$HOME/opt/cmake/bin/cmake" --version
"$HOME/opt/ninja" --version
