from pathlib import Path
import math
import re

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
    p = ROOT / f"_perf_out_unordered_flat_{name}.txt"
    d = parse(p)
    print(f"=== {name} ({p.stat().st_size} B) ===")
    print(f"{'op':10} {'xer':>8} {'boost':>8} {'absl':>8} {'x/b':>6} {'x/a':>6}")
    for op in ("insert", "reinsert", "find_hit", "find_miss", "erase_half"):
        xs, bs, a_s, xb, xa = [], [], [], [], []
        for (_tag, o), v in d.items():
            if o != op:
                continue
            xs.append(v[1])
            bs.append(v[2])
            a_s.append(v[3])
            if v[2]:
                xb.append(v[1] / v[2])
            if v[3]:
                xa.append(v[1] / v[3])
        print(
            f"{op:10} {gm(xs):8.3f} {gm(bs):8.3f} {gm(a_s):8.3f} "
            f"{gm(xb):6.3f} {gm(xa):6.3f}"
        )
    print("-- find_hit samples --")
    for tag in (
        "set_i32/hash_mix",
        "set_i32/hash_xxh3",
        "set_u64/hash_mix",
        "map_i32_i32/hash_mix",
        "map_str_str/hash_mix",
        "map_pod_u64/hash_mix",
    ):
        k = (tag, "find_hit")
        if k in d:
            v = d[k]
            print(
                f"{tag:24} xer={v[1]:7.3f} boost={v[2]:7.3f} "
                f"absl={v[3]:7.3f} x/b={v[1]/v[2]:.3f}"
            )
    print()
