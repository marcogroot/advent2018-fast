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
| 1 | 14.0 μs | 11.8 μs | **2.5 μs (4.7x)** |
| 2 | 13.0 μs | 8.7 μs | **1.5 μs (5.8x)** |
| 3 | 85.3 μs | 54.2 μs | **29.3 μs (1.8x)** |
| 4 | 30.4 μs | 24.3 μs | **2.1 μs (11.6x)** |
| 5 | 256.4 μs | 217.3 μs | **28.9 μs (7.5x)** |
| 6 | 415.2 μs | 365.6 μs | **5.6 μs (65.3x)** |
| 7 | 0.9 μs | 0.4 μs | **0.2 μs (2.0x)** |
| 8 | 26.1 μs | 22.0 μs | **6.2 μs (3.5x)** |
| 9 | 1.20 ms | 1.80 ms | **277.5 μs (6.5x)** |
| 10 | 13.6 μs | 8.2 μs | **0.8 μs (10.2x)** |
| 11 | 1.01 ms | 602.9 μs | **121.2 μs (5.0x)** |
| 12 | 42.3 μs | 38.0 μs | **2.3 μs (16.5x)** |
| 13 | 192.6 μs | 190.5 μs | **37.1 μs (5.1x)** |
| 14 | 29.58 ms | 45.90 ms | **2.30 ms (19.9x)** |
| 15 | 6.44 ms | 1.81 ms | **594.1 μs (3.0x)** |
| 16 | 44.0 μs | 35.7 μs | **4.2 μs (8.5x)** |
| 17 | 519.8 μs | 305.8 μs | **24.1 μs (12.7x)** |
| 18 | 1.05 ms | 334.1 μs | **25.4 μs (13.2x)** |
| 19 | 0.6 μs | 0.4 μs | **0.2 μs (2.0x)** |
| 20 | 69.1 μs | 18.8 μs | **11.6 μs (1.6x)** |
| 21 | 52.8 μs | 53.5 μs | **23.8 μs (2.2x)** |
| 22 | 2.75 ms | 1.85 ms | **74.0 μs (25.0x)** |
| 23 | 126.4 μs | 94.9 μs | **19.5 μs (4.9x)** |
| 24 | 4.70 ms | 3.72 ms | **1.91 ms (1.9x)** |
| 25 | 256.3 μs | 165.2 μs | **10.2 μs (16.2x)** |
| **Total** | **48.88 ms** | **57.63 ms** | **5.51 ms (10.5x)** |

Min time per day, single-threaded. Speedups are vs `original` (native arm64). Bold = fastest per day. Machine: Apple M4 Max, macOS 26.7.1.
<!-- results:end -->

## Testing a new agent

```bash
shared/new-solution.sh my-agent        # copies original/src into my-agent/src
# give the agent shared/AGENT_PROMPT.md with <name> = my-agent, let it work
shared/verify.sh my-agent              # must print ALL PASS
shared/bench.sh my-agent               # writes shared/results/my-agent.tsv (run on an idle machine)
shared/compare.py --readme             # regenerates the table above
```

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
