from pathlib import Path
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
        tag, op = m.group(1), m.group(2)
        cols = [float(m.group(i).split("/")[0]) for i in range(3, 7)]
        d[(tag, op)] = cols  # std, xer, boost, absl
    return d


def summarize(name: str) -> None:
    old = parse(ROOT / f"_perf_out_unordered_flat_{name}_insert_ab_old.txt")
    new = parse(ROOT / f"_perf_out_unordered_flat_{name}_insert_ab_new.txt")
    print(f"=== {name}: new vs old (neg% = new faster) ===")
    print(
        f"{'tag':22} {'op':10} {'old':>8} {'new':>8} {'d%':>7} "
        f"{'old/b':>7} {'new/b':>7}"
    )
    by_op = {op: [] for op in ("insert", "reinsert", "find_hit", "find_miss", "erase_half")}
    for k in sorted(new):
        if k not in old:
            continue
        op = k[1]
        if op not in by_op:
            continue
        xo, xn = old[k][1], new[k][1]
        bo, bn = old[k][2], new[k][2]
        pct = (xn - xo) / xo * 100 if xo else 0.0
        by_op[op].append(pct)
        if op in ("insert", "reinsert") or abs(pct) >= 5:
            r0 = xo / bo if bo else 0.0
            r1 = xn / bn if bn else 0.0
            print(
                f"{k[0]:22} {op:10} {xo:8.3f} {xn:8.3f} {pct:+6.1f}% "
                f"{r0:7.3f} {r1:7.3f}"
            )
    print("-- averages --")
    for op, xs in by_op.items():
        if not xs:
            continue
        wins = sum(1 for x in xs if x < -3)
        loss = sum(1 for x in xs if x > 3)
        print(
            f"{op:10} avg {stats.mean(xs):+.1f}%  "
            f"med {stats.median(xs):+.1f}%  "
            f"faster={wins} slower={loss} /{len(xs)}"
        )
    print()


for n in ("msvc", "gcc", "clang"):
    summarize(n)
