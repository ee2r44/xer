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
    no = parse(ROOT / f"_perf_out_unordered_flat_{name}_pf_ab_no.txt")
    yes = parse(ROOT / f"_perf_out_unordered_flat_{name}_pf_ab_yes.txt")
    print(f"=== {name}: prefetch YES vs NO (neg% = yes faster) ===")
    print(
        f"{'tag':22} {'op':10} {'no':>8} {'yes':>8} {'d%':>7} "
        f"{'no/b':>7} {'yes/b':>7}"
    )
    by = {op: [] for op in ("insert", "reinsert", "find_hit", "find_miss", "erase_half")}
    xb_no = {op: [] for op in by}
    xb_yes = {op: [] for op in by}
    for k in sorted(yes):
        if k not in no:
            continue
        op = k[1]
        xo, xn = no[k][1], yes[k][1]
        bo, bn = no[k][2], yes[k][2]
        pct = (xn - xo) / xo * 100 if xo else 0.0
        by[op].append(pct)
        if bo:
            xb_no[op].append(xo / bo)
        if bn:
            xb_yes[op].append(xn / bn)
        if op in ("find_hit", "find_miss") or abs(pct) >= 5:
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
    print("-- xer/boost geo-mean --")
    for op in ("find_hit", "find_miss"):
        print(f"{op:10} no/b {gm(xb_no[op]):.3f}  yes/b {gm(xb_yes[op]):.3f}")
    print()
