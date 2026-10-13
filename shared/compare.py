#!/usr/bin/env python3
"""Compare benchmark results in shared/results/*.tsv.

usage: shared/compare.py [--readme]   (--readme also rewrites the results table in README.md)
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


def fmt(us):
    return f"{us / 1000:,.2f} ms" if us >= 1000 else f"{us:,.1f} μs"


def table():
    results = {p.stem: load(p) for p in sorted((SHARED / "results").glob("*.tsv"))}
    names = [n for n in FIRST if n in results] + sorted(n for n in results if n not in FIRST)
    base = results.get(BASELINE, (None, None))[1]
    header = "| Day | " + " | ".join(f"`{n}`" for n in names) + " |"
    lines = [header, "|----:|" + "---:|" * len(names)]
    totals = {n: sum(results[n][1].values()) for n in names}
    for day in range(1, 26):
        best = min(results[n][1][day] for n in names if day in results[n][1])
        cells = []
        for n in names:
            v = results[n][1].get(day)
            if v is None:
                cells.append("–")
                continue
            cell = fmt(v)
            if base and n not in FIRST:
                cell += f" ({base[day] / v:.1f}x)"
            cells.append(f"**{cell}**" if v == best else cell)
        lines.append(f"| {day} | " + " | ".join(cells) + " |")
    total_cells = []
    best_total = min(totals.values())
    for n in names:
        cell = fmt(totals[n])
        if base and n not in FIRST:
            cell += f" ({sum(base.values()) / totals[n]:.1f}x)"
        total_cells.append(f"**{cell}**")
    lines.append("| **Total** | " + " | ".join(total_cells) + " |")
    machines = sorted({results[n][0].get("machine", "?") for n in names})
    lines.append("")
    lines.append(f"Min time per day, single-threaded. Speedups are vs `{BASELINE}` (native arm64). Bold = fastest per day. Machine: {'; '.join(machines)}.")
    return "\n".join(lines)


def main():
    out = table()
    print(out)
    if "--readme" in sys.argv:
        readme = ROOT / "README.md"
        text = readme.read_text()
        start, end = "<!-- results:start -->", "<!-- results:end -->"
        i, j = text.index(start) + len(start), text.index(end)
        readme.write_text(text[:i] + "\n" + out + "\n" + text[j:])


if __name__ == "__main__":
    main()
