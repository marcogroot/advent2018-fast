# Agent prompt template

Copy this to the agent you want to test, replacing `<name>` with its solution folder (create it first with `shared/new-solution.sh <name>`).
For one agent per day, replace "every day" with "day NN"; each agent only runs `shared/test.sh <name> NN`, and you do the full run once they are all done.

---

You are optimizing Advent of Code 2018 solutions for speed on this machine (single-threaded). Repo: the current directory.
Your solution lives in `<name>/src/`. It starts as a copy of `original/src/`, Voltara's heavily optimized AVX2 C++ code
(see `original/README.md` for notes on each day). On arm64 that code builds through SIMDe, an AVX2-to-NEON translation layer,
which is often far from optimal. Goal: make every day in `<name>/src/` as fast as possible.

The only things you do: edit `.cpp` files in `<name>/src/`, then run the shared test command from the repo root.
It builds, verifies against the original's answers on 205 public inputs (shared/tests/), and benchmarks against the original for you:

- `shared/test.sh <name> 14`        quick loop for one or more days: build, verify, and an A/B timing vs original.
                                     Other days link from original/, so it's safe while other agents edit other days.
- `shared/test.sh <name>`           final full run: verifies all 25 days, benchmarks 3 rounds, writes
                                     shared/results/<name>.tsv and updates the README results table.

Run the full run once at the end. Any FAIL line means your output differs from the original for that input.
Timings are noisy if other work is running; judge by the min and re-run if unsure.

Rules:
1. Edit only files under `<name>/`. Do not modify `original/`, `shared/`, or other solutions.
   `advent2018.h` and the harness interface (`void dayNN(input_t)`) must stay compatible with `shared/bench.cpp`.
2. Output must be byte-identical to the original's (same printf lines); `shared/verify.sh <name>` must pass for every input.
3. Genuine general solutions only: no hardcoded answers, no input-specific constants, no tables encoding puzzle results,
   no compile-time precomputation of the puzzle's simulation. Small generic tables (bit tricks, shuffle patterns) are fine.
4. All work happens inside `dayNN(input)` on every call: no state cached across calls, no threads, no work at static-init time.
5. The input is a read-only mmap; reading past the end is safe (zero backsplash), writing is not.
6. C++17, clang, flags fixed by `shared/common.sh` (-O3 -mcpu=native). NEON intrinsics (`<arm_neon.h>`) are allowed.

When done, write a short summary per day (what changed, what didn't help) in `<name>/NOTES.md`.
