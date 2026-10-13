# opus-5.5

Agent/model: Claude Opus 5.5 (`claude-opus-5-5`), one subagent per day, orchestrated by Claude Code.
Date: 2026-10-13
Machine: Apple M4 Max

## Time taken

| | |
|---|---|
| Setup (fork, ARM build, harness, correctness tests) | ~6 min (fork created 13:35, agents launched 13:42), done by the orchestrating session |
| Agents running (first launched to last finished) | **~29 min** wall clock (13:42 → 14:11), up to 20 agents at a time |
| **Fork to all 25 days done** | **~36 min** (13:35 → 14:11 AEDT, 13 Oct 2026) |
| Total agent time (sum of all 25 agents) | 5.2 h |
| Total agent tokens | 3,277,082 |
| Final verify + benchmark | ~2 min |

Per-day agent stats (from each subagent's completion report):

| Day | Agent time | Tokens | Tool calls |
|----:|---:|---:|---:|
| 1 | 5m 51s | 87,179 | 22 |
| 2 | 9m 50s | 115,468 | 43 |
| 3 | 12m 58s | 137,844 | 44 |
| 4 | 8m 18s | 109,120 | 42 |
| 5 | 7m 56s | 99,669 | 32 |
| 6 | 14m 36s | 158,970 | 42 |
| 7 | 16m 47s | 152,123 | 58 |
| 8 | 3m 13s | 69,243 | 15 |
| 9 | 13m 16s | 132,610 | 33 |
| 10 | 7m 56s | 117,627 | 35 |
| 11 | 5m 48s | 87,698 | 19 |
| 12 | 9m 19s | 96,939 | 27 |
| 13 | 13m 29s | 129,593 | 42 |
| 14 | 23m 48s | 199,666 | 78 |
| 15 | 11m 18s | 123,075 | 31 |
| 16 | 16m 18s | 167,091 | 75 |
| 17 | 9m 17s | 112,822 | 43 |
| 18 | 12m 54s | 140,055 | 36 |
| 19 | 10m 19s | 111,967 | 31 |
| 20 | 21m 08s | 185,892 | 54 |
| 21 | 11m 10s | 113,923 | 34 |
| 22 | 22m 27s | 180,348 | 63 |
| 23 | 16m 05s | 159,466 | 60 |
| 24 | 12m 53s | 131,400 | 46 |
| 25 | 15m 05s | 157,294 | 62 |
| **Total** | **312m 02s** | **3,277,082** | **1067** |

## Per-day notes

- **Day 1:** Hash table grouping prefix sums by remainder replaces two `std::sort`s; two-pass NEON newline scan + branch-free line parse.
- **Day 2:** Part 1: four words transposed into NEON lanes, one-hot letter counts with `vbslq`. Part 2: hash of each word minus one of 4 chunks finds the near-duplicate (~16 compares instead of ~780). Both fused in one loop.
- **Day 3:** Claims counting-sorted into 16-row bands (stays in L1); `seen`/`collide` bitplanes updated with one NEON RMW per claim row; SWAR number parse.
- **Day 4:** No sort or hash map: per-day 64-bit sleep masks (prefix-XOR of toggles), fixed-offset parse, per-guard minute counters kept in NEON registers.
- **Day 5:** Part 1: 8 interleaved branchless stacks merged at the end. Part 2: stack of (start,end) intervals into the reduced polymer, NEON to find cancel length.
- **Day 6:** Flood fill replaced with a per-row sweep over x-sorted points (switch-over cell computed directly), 16 rows at a time with NEON and per-batch point pruning. Part 2 via monotone two-pointer walk, no sort.
- **Day 7:** Indegrees as 5 bit-planes; finishing a node subtracts its successor mask branchlessly. Parts 1 and 2 fused in one 26-step loop.
- **Day 8:** NEON parse of 16 bytes at a time into a byte array; iterative stack-based tree walk with masked NEON metadata sums; no zero-filled vectors.
- **Day 9:** 23-marble rounds rewritten in native NEON zips; rounds whose output is only needed for scoring store nothing (57% of rounds), unrolled by their 16-round cycle.
- **Day 10:** Fixed-layout NEON line parser (one table lookup per line) with fallback; slowest-star search fused into the parse loop.
- **Day 11:** 16-bit wrapping summed-area table (exact for square sums), mod-free grid build, find-max-then-locate search over 4 sizes per pass.
- **Day 12:** Bitsliced NEON automaton (31 `vbslq` per 128 cells) with a fused shifted-equality stability check; part 2 extrapolated once the pattern only shifts.
- **Day 13:** Carts jump node-to-node on a precomputed track graph; all carts advance (D-1)/2 ticks at once when the closest pair is D apart.
- **Day 14:** NEON table-lookup recipe expansion into a 512 KB rolling buffer; mixer chain propagated by 12 speculative chains merged branchlessly; 3-digit NEON prefilter for part 2. Also fixes the original's part 1 output when the part 2 answer is smaller than the input.
- **Day 15:** Map and BFS frontiers kept in 8 NEON registers, BFS limited to reachable rows; part 2 resumes from part-1 snapshots and skips powers with identical hits-to-kill.
- **Day 16:** Fixed-offset sample parse in 4 independent chains; NEON opcode matching on 16 samples at once; VM with registers in CPU registers and a 1024-case switch.
- **Day 17:** Row-wise water spreading on a bit-packed grid (two masks per row); sealed rows filled in one pass; counts maintained incrementally.
- **Day 18:** Bitboards with bitwise full adders; rows split between NEON and scalar units to keep both busy; hashed cycle detection.
- **Day 19:** Instruction pointer kept in a register; opcode via 256-byte hash table; NEON newline scan for parallel line parsing.
- **Day 20:** Standard grid-distance walk made branchless with a 256-entry per-character table and `csel`; 128x128 u16 grid (real maps are 100x100).
- **Day 21:** Constants read from the parsed elfcode (with a general fallback); step rewritten to shorten the multiply dependency chain; 16-checkpoint NEON cycle detection.
- **Day 22:** Bitset Dijkstra stepped one minute at a time with NEON dilation and an 8-slot ring for tool switches; NEON wavefront erosion-level generation.
- **Day 23:** Per-node three-way partitioning replaces full sorts of split points; NEON parse and 16-bot-at-a-time in-range masks.
- **Day 24:** Target selection is one count-trailing-zeros on per-rank bitmasks; endgame shortcut once one side can no longer kill anything.
- **Day 25:** NEON parse; points binned into a fixed (x,y) cell grid and checked only against neighbour cells, 16 at a time; union-find in a separate pass.

## Assumptions beyond the original

- Day 20 assumes the map spans fewer than 128 rooms per axis (original: 256). Real inputs are 100x100.
- Day 22 gives the true shortest path when depth % 3 != 0, where the original's part 2 is wrong. Every known real input has depth % 3 == 0, where both agree.
