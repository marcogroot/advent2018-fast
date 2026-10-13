#!/usr/bin/env python3
"""Compare benchmark results.

Baselines live in shared/results/{original,original-x86}.tsv; each solution's results live in <solution>/results.tsv.

usage:
  shared/compare.py                    all solutions side by side
  shared/compare.py --only NAME        one solution vs the baselines only (what test.sh prints to agents)
  shared/compare.py --out FILE         also write the table to FILE (markdown)
"""
import pathlib
import sys

SHARED = pathlib.Path(__file__).resolve().parent
ROOT = SHARED.parent
BASELINE = "original"
FIRST = ["original-x86", "original"]


def load(path):
    meta, days = {}, {}
    for line in path.read_text().splitlines():
        if line.startswith("#"):
            key, _, value = line[1:].partition(":")
            meta[key.strip()] = value.strip()
        elif line and not line.startswith("day"):
            day, min_us, _ = line.split("\t")
            days[int(day)] = float(min_us)
    return meta, days


def all_results():
    results = {p.stem: load(p) for p in sorted((SHARED / "results").glob("*.tsv"))}
    for p in sorted(ROOT.glob("*/results.tsv")):
        if p.parent.name not in FIRST:
            results[p.parent.name] = load(p)
    return results


def fmt(us):
    return f"{us / 1000:,.2f} ms" if us >= 1000 else f"{us:,.1f} μs"


def table(results):
    names = [n for n in FIRST if n in results] + sorted(n for n in results if n not in FIRST)
    base = results.get(BASELINE, (None, None))[1]
    lines = ["| Day | " + " | ".join(f"`{n}`" for n in names) + " |", "|----:|" + "---:|" * len(names)]
    for day in range(1, 26):
        present = [results[n][1][day] for n in names if day in results[n][1]]
        best = min(present) if present else None
        cells = []
        for n in names:
            v = results[n][1].get(day)
            if v is None:
                cells.append("–")
                continue
            cell = fmt(v) + (f" ({base[day] / v:.1f}x)" if base and n not in FIRST else "")
            cells.append(f"**{cell}**" if v == best else cell)
        lines.append(f"| {day} | " + " | ".join(cells) + " |")
    totals = []
    for n in names:
        t = sum(results[n][1].values())
        totals.append(f"**{fmt(t)}" + (f" ({sum(base.values()) / t:.1f}x)" if base and n not in FIRST else "") + "**")
    lines.append("| **Total** | " + " | ".join(totals) + " |")
    machines = sorted({results[n][0].get("machine", "?") for n in names})
    lines += ["", f"Min time per day, single-threaded. Speedups are vs `{BASELINE}` (native arm64). Bold = fastest per day. Machine: {'; '.join(machines)}."]
    return "\n".join(lines)


def main():
    args = sys.argv[1:]
    results = all_results()
    if "--only" in args:
        name = args[args.index("--only") + 1]
        results = {n: r for n, r in results.items() if n in FIRST or n == name}
    out = table(results)
    print(out)
    if "--out" in args:
        pathlib.Path(args[args.index("--out") + 1]).write_text(out + "\n")


if __name__ == "__main__":
    main()
