from pathlib import Path
import re
import statistics as stats

ROOT = Path(__file__).resolve().parents[1]
LIBS = ("std", "xer", "boost", "absl")
OPS = ("insert", "find_hit", "find_miss", "erase_half", "reinsert")


def parse(path: Path):
    d = {}
    raw = path.read_bytes()
    if raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff"):
        text = raw.decode("utf-16")
    else:
        text = raw.decode("utf-8", errors="replace")
    for line in text.splitlines():
        m = re.match(
            r"(\S+)\s+(insert|find_hit|find_miss|erase_half|reinsert)\s+"
            r"(\S+)\s+(\S+)\s+(\S+)\s+(\S+)",
            line,
        )
        if not m:
            continue
        tag, op = m.group(1), m.group(2)
        cols = [float(m.group(i).split("/")[0]) for i in range(3, 7)]
        d[(tag, op)] = dict(zip(LIBS, cols))
    return d


def geo_mean(xs):
    import math

    xs = [x for x in xs if x > 0]
    if not xs:
        return float("nan")
    return math.exp(sum(math.log(x) for x in xs) / len(xs))


def summarize(name: str) -> None:
    d = parse(ROOT / f"_perf_out_unordered_flat_{name}.txt")
    print(f"=== {name}  ns/op median; ratios xer/peer ( <1 xer faster) ===")
    print(
        f"{'tag':22} {'op':10} {'std':>8} {'xer':>8} {'boost':>8} {'absl':>8} "
        f"{'x/std':>6} {'x/bst':>6} {'x/absl':>6}"
    )
    by_op = {op: {lib: [] for lib in LIBS} for op in OPS}
    ratios = {op: {"std": [], "boost": [], "absl": []} for op in OPS}
    for (tag, op), row in sorted(d.items()):
        if op not in by_op:
            continue
        for lib in LIBS:
            by_op[op][lib].append(row[lib])
        xs, xx, xb, xa = row["std"], row["xer"], row["boost"], row["absl"]
        r_std = xx / xs if xs else float("nan")
        r_bst = xx / xb if xb else float("nan")
        r_absl = xx / xa if xa else float("nan")
        ratios[op]["std"].append(r_std)
        ratios[op]["boost"].append(r_bst)
        ratios[op]["absl"].append(r_absl)
        print(
            f"{tag:22} {op:10} {xs:8.3f} {xx:8.3f} {xb:8.3f} {xa:8.3f} "
            f"{r_std:6.2f} {r_bst:6.2f} {r_absl:6.2f}"
        )
    print("-- xer vs peers: geo-mean of (xer/peer); count xer faster (<0.97) --")
    for op in OPS:
        parts = []
        for peer in ("std", "boost", "absl"):
            rs = ratios[op][peer]
            gm = geo_mean(rs)
            wins = sum(1 for r in rs if r < 0.97)
            loss = sum(1 for r in rs if r > 1.03)
            parts.append(f"{peer} {gm:.2f} (w{wins}/l{loss}/{len(rs)})")
        print(f"{op:10} " + " | ".join(parts))
    print()


for n in ("msvc", "gcc", "clang"):
    summarize(n)
