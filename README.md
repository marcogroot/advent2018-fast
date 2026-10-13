# advent2018-fast: agent speed-up benchmark

A testbed for comparing AI coding agents on the same task: take an already heavily optimized set of
[Advent of Code 2018](https://adventofcode.com/2018) solutions, [Voltara/advent2018-fast](https://github.com/Voltara/advent2018-fast),
and make them faster. Each agent works in its own folder, in its own way. Everyone is benchmarked with the same harness and checked against the same answers.

## Layout

| Folder | What it is |
|---|---|
| `original/` | The starting point: Voltara's AVX2 C++ code, plus minimal fixes so it compiles with modern clang and runs natively on arm64 via [SIMDe](https://github.com/simd-everywhere/simde). Its `README.md` is the upstream one with notes on each day. |
| `opus-5.5/` | Claude Opus 5.5's solutions. Agents being tested should not read this folder: it contains Opus's results. |
| `shared/` | Benchmark harness, build/verify/bench scripts, agent prompt template, and `results/` with the original's baseline timings. |

## Testing a new agent

Agents only edit `.cpp` files in their own folder, then run one shared command that builds, verifies and benchmarks for them.

```bash
shared/new-solution.sh my-agent        # copies original/src into my-agent/src
# give the agent shared/AGENT_PROMPT.md with <name> = my-agent
shared/test.sh my-agent 14             # while iterating: build + verify + A/B vs original for day 14
shared/test.sh my-agent                # final: verify all days, benchmark, write my-agent/results.tsv
shared/compare.py --out results.md     # (you, not the agent) compare every solution side by side
```

Under the hood, `test.sh` uses `build.sh`, `bday.sh`, `verify.sh`, `bench.sh` and `compare.py` in `shared/`; you don't normally need to call those directly.
Each solution's results live in its own folder (`<name>/results.tsv`), so an agent only ever sees its own numbers and the original's. Run final benchmarks on an idle machine so results are comparable.

## Setup

Requires macOS clang, plus `brew install boost simde`. The x86 baseline (`shared/bench.sh original --x86`) needs Rosetta 2.

Puzzle inputs are not committed, because AoC asks people not to redistribute them. To recreate them:

```bash
git clone --depth 1 https://github.com/badouralix/adventofcode-2018 /tmp/aoc2018-inputs
shared/make_tests.sh /tmp/aoc2018-inputs          # 205 inputs from ~27 people + the original's answers
for d in $(seq -w 1 25); do cp shared/tests/day$d/evqna.txt shared/input/day$d.txt 2>/dev/null; done
cp shared/tests/day10/ayoub.txt shared/input/day10.txt   # evqna has no day 10
```

`shared/input/dayNN.txt` is the benchmark input; `shared/tests/` holds the correctness inputs. Every input is normalized to end with a newline, as real AoC downloads do.

## Rules for solutions

Single-threaded, output identical to the original's, a general solution for any valid input (no hardcoded answers or precomputed simulation),
and all work done inside each `dayNN()` call. Full rules are in `shared/AGENT_PROMPT.md`.
