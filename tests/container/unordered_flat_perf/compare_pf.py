from pathlib import Path
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
            r"(\S+)\s+(find_hit|find_miss|insert)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)",
            line,
        )
        if not m:
            continue
        tag, op = m.group(1), m.group(2)
        cols = [float(m.group(i).split("/")[0]) for i in range(3, 7)]
        d[(tag, op)] = cols  # std, xer, boost, absl
    return d


def report(name: str, baseline_suffix: str, label: str) -> None:
    before = parse(ROOT / f"_perf_out_unordered_flat_{name}_{baseline_suffix}.txt")
    after = parse(ROOT / f"_perf_out_unordered_flat_{name}.txt")
    print(f"=== {name} vs {label} ===")
    print(
        f"{'tag':22} {'op':10} {'xer0':>8} {'xer1':>8} {'d%':>7} "
        f"{'x/b0':>7} {'x/b1':>7}"
    )
    fh_d = []
    fm_d = []
    fh_rel = []
    for k in sorted(after):
        if k[1] not in ("find_hit", "find_miss") or k not in before:
            continue
        b, a = before[k], after[k]
        xb, xa = b[1], a[1]
        pct = (xa - xb) / xb * 100 if xb else 0.0
        r0 = xb / b[2] if b[2] else 0.0
        r1 = xa / a[2] if a[2] else 0.0
        if k[1] == "find_hit":
            fh_d.append(pct)
            fh_rel.append((r1 - r0) / r0 * 100 if r0 else 0.0)
        else:
            fm_d.append(pct)
        if abs(pct) >= 5 or k[1] == "find_hit":
            print(
                f"{k[0]:22} {k[1]:10} {xb:8.3f} {xa:8.3f} {pct:+6.1f}% "
                f"{r0:7.3f} {r1:7.3f}"
            )
    if fh_d:
        avg = sum(fh_d) / len(fh_d)
        wins = sum(1 for x in fh_d if x < -3)
        loss = sum(1 for x in fh_d if x > 3)
        print(
            f"find_hit xer abs: avg {avg:+.1f}%  "
            f"faster={wins} slower={loss} /{len(fh_d)}"
        )
        print(
            f"find_hit xer/boost ratio change avg "
            f"{sum(fh_rel)/len(fh_rel):+.1f}% (neg=closer to boost)"
        )
    if fm_d:
        avg = sum(fm_d) / len(fm_d)
        wins = sum(1 for x in fm_d if x < -3)
        loss = sum(1 for x in fm_d if x > 3)
        print(
            f"find_miss xer abs: avg {avg:+.1f}%  "
            f"faster={wins} slower={loss} /{len(fm_d)}"
        )
    print()


for n in ("msvc", "gcc", "clang"):
    report(n, "before_pf", "no-prefetch baseline")
    report(n, "pf1", "prev unit-prefetch")
