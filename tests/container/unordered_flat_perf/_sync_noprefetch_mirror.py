from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
src = root / "include"
dst = root / "tests/container/ab_mirror/find_noprefetch/include"
if dst.exists():
    shutil.rmtree(dst)
shutil.copytree(src, dst)
p = dst / "xer/container/xer_unordered_flat_table.h"
t = p.read_text(encoding="utf-8")
old = "\t\t\t\t\t\tXER_PREFETCH(ppu); // x86 对齐 Boost：只预取首行；或 XER_PREFETCH_N(ppu, 15)\n"
if old not in t:
    raise SystemExit("prefetch line not found")
t = t.replace(old, "", 1)
if "XER_PREFETCH" in t:
    raise SystemExit("strip incomplete")
p.write_text(t, encoding="utf-8", newline="\n")
print("mirror ok (ungated pf stripped)")
