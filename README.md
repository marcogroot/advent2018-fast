# advent2018-fast: agent speed-up benchmark

A testbed for comparing AI coding agents on the same task: take an already heavily optimized set of
[Advent of Code 2018](https://adventofcode.com/2018) solutions, [Voltara/advent2018-fast](https://github.com/Voltara/advent2018-fast),
and make them faster. Each agent works in its own folder, in its own way. Everyone is benchmarked with the same harness and checked against the same answers.

## Layout

| Folder | What it is |
|---|---|
| `original/` | The starting point: Voltara's AVX2 C++ code, plus minimal fixes so it compiles with modern clang and runs natively on arm64 via [SIMDe](https://github.com/simd-everywhere/simde). Its `README.md` is the upstream one with notes on each day. |
| `opus-5.5/` | Claude Opus 5.5's optimized solutions (one subagent per day). See `opus-5.5/NOTES.md`. |
| `shared/` | Benchmark harness, build/verify/bench scripts, agent prompt template, and `results/`, one TSV per solution. |

## Results

<!-- results:start -->
| Day | `original-x86` | `original` | `opus-5.5` |
|----:|---:|---:|---:|
| 1 | 14.0 μs | 11.8 μs | **2.3 μs (5.1x)** |
| 2 | 13.0 μs | 8.7 μs | **1.4 μs (6.2x)** |
| 3 | 85.3 μs | 54.2 μs | **26.4 μs (2.1x)** |
| 4 | 30.4 μs | 24.3 μs | **2.1 μs (11.6x)** |
| 5 | 256.4 μs | 217.3 μs | **26.4 μs (8.2x)** |
| 6 | 415.2 μs | 365.6 μs | **5.6 μs (65.3x)** |
| 7 | 0.9 μs | 0.4 μs | **0.2 μs (2.0x)** |
| 8 | 26.1 μs | 22.0 μs | **5.8 μs (3.8x)** |
| 9 | 1.20 ms | 1.80 ms | **270.0 μs (6.7x)** |
| 10 | 13.6 μs | 8.2 μs | **0.8 μs (10.2x)** |
| 11 | 1.01 ms | 602.9 μs | **130.8 μs (4.6x)** |
| 12 | 42.3 μs | 38.0 μs | **2.1 μs (18.1x)** |
| 13 | 192.6 μs | 190.5 μs | **37.0 μs (5.1x)** |
| 14 | 29.58 ms | 45.90 ms | **2.27 ms (20.2x)** |
| 15 | 6.44 ms | 1.81 ms | **542.0 μs (3.3x)** |
| 16 | 44.0 μs | 35.7 μs | **3.8 μs (9.4x)** |
| 17 | 519.8 μs | 305.8 μs | **23.9 μs (12.8x)** |
| 18 | 1.05 ms | 334.1 μs | **25.7 μs (13.0x)** |
| 19 | 0.6 μs | 0.4 μs | **0.2 μs (2.0x)** |
| 20 | 69.1 μs | 18.8 μs | **11.5 μs (1.6x)** |
| 21 | 52.8 μs | 53.5 μs | **23.2 μs (2.3x)** |
| 22 | 2.75 ms | 1.85 ms | **72.9 μs (25.4x)** |
| 23 | 126.4 μs | 94.9 μs | **17.4 μs (5.5x)** |
| 24 | 4.70 ms | 3.72 ms | **1.92 ms (1.9x)** |
| 25 | 256.3 μs | 165.2 μs | **10.2 μs (16.2x)** |
| **Total** | **48.88 ms** | **57.63 ms** | **5.44 ms (10.6x)** |

Min time per day, single-threaded. Speedups are vs `original` (native arm64). Bold = fastest per day. Machine: Apple M4 Max, macOS 26.7.1.
<!-- results:end -->

## Testing a new agent

Agents only edit `.cpp` files in their own folder, then run one shared command that builds, verifies and benchmarks for them.

```bash
shared/new-solution.sh my-agent        # copies original/src into my-agent/src
# give the agent shared/AGENT_PROMPT.md with <name> = my-agent
shared/test.sh my-agent 14             # while iterating: build + verify + A/B vs original for day 14
shared/test.sh my-agent                # final: verify all days, benchmark, write shared/results/my-agent.tsv, update table above
```

Under the hood, `test.sh` uses `build.sh`, `bday.sh`, `verify.sh`, `bench.sh` and `compare.py` in `shared/`; you don't normally need to call those directly.
Run final benchmarks on an idle machine so results are comparable.

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
