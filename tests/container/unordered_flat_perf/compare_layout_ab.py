from pathlib import Path
import math
import re
import statistics as stats

ROOT = Path(__file__).resolve().parents[1]


def parse(path: Path):
    d = {}
    raw = path.read_bytes()
    if raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff"):
        text = raw.decode("utf-16")
    else:
        text = raw.decode("utf-8", errors="replace")
    for line in text.splitlines():
        m = re.match(
            r"(\S+)\s+(insert|reinsert|find_hit|find_miss|erase_half)\s+"
            r"(\S+)\s+(\S+)\s+(\S+)\s+(\S+)",
            line,
        )
        if not m:
            continue
        d[(m.group(1), m.group(2))] = [
            float(m.group(i).split("/")[0]) for i in range(3, 7)
        ]
    return d


def gm(xs):
    xs = [x for x in xs if x > 0]
    if not xs:
        return float("nan")
    return math.exp(sum(math.log(x) for x in xs) / len(xs))


for name in ("msvc", "gcc", "clang"):
    old = parse(ROOT / f"_perf_out_unordered_flat_{name}_layout_ab_old.txt")
    new = parse(ROOT / f"_perf_out_unordered_flat_{name}_layout_ab_new.txt")
    print(f"=== {name}: NEW interleaved vs OLD SoA (neg% = new faster) ===")
    print(
        f"{'tag':22} {'op':10} {'old':>8} {'new':>8} {'d%':>7} "
        f"{'old/b':>7} {'new/b':>7}"
    )
    by = {op: [] for op in ("insert", "reinsert", "find_hit", "find_miss", "erase_half")}
    xb_old = {op: [] for op in by}
    xb_new = {op: [] for op in by}
    for k in sorted(new):
        if k not in old:
            continue
        op = k[1]
        xo, xn = old[k][1], new[k][1]
        bo, bn = old[k][2], new[k][2]
        pct = (xn - xo) / xo * 100 if xo else 0.0
        by[op].append(pct)
        if bo:
            xb_old[op].append(xo / bo)
        if bn:
            xb_new[op].append(xn / bn)
        if op == "find_hit" or (op in ("insert", "reinsert") and abs(pct) >= 8):
            r0 = xo / bo if bo else 0.0
            r1 = xn / bn if bn else 0.0
            print(
                f"{k[0]:22} {op:10} {xo:8.3f} {xn:8.3f} {pct:+6.1f}% "
                f"{r0:7.3f} {r1:7.3f}"
            )
    print("-- xer abs --")
    for op, xs in by.items():
        wins = sum(1 for x in xs if x < -3)
        loss = sum(1 for x in xs if x > 3)
        print(
            f"{op:10} avg {stats.mean(xs):+.1f}%  med {stats.median(xs):+.1f}%  "
            f"faster={wins} slower={loss} /{len(xs)}"
        )
    print("-- xer/boost ratio geo-mean (<1 xer ahead) --")
    for op in ("insert", "find_hit", "find_miss", "reinsert"):
        print(f"{op:10} old/b {gm(xb_old[op]):.3f}  new/b {gm(xb_new[op]):.3f}")
    print()
