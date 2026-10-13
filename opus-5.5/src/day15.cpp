#include "advent2018.h"
#include <arm_neon.h>

namespace {

struct alignas(16) Map {
	uint32_t r[32];

	void clear() { memset(r, 0, sizeof(r)); }
	bool test(int pos) const { return (r[pos >> 5] >> (pos & 31)) & 1; }
	void toggle(int pos) { r[pos >> 5] ^= 1u << (pos & 31); }
};

struct V8 {
	uint32x4_t v[8];
};

static inline V8 load(const Map &m) {
	V8 a;
	for (int i = 0; i < 8; i++) a.v[i] = vld1q_u32(m.r + 4 * i);
	return a;
}

static inline V8 single(int pos) {
	alignas(16) uint32_t tmp[32] = {};
	tmp[pos >> 5] = 1u << (pos & 31);
	V8 a;
	for (int i = 0; i < 8; i++) a.v[i] = vld1q_u32(tmp + 4 * i);
	return a;
}

static inline V8 adjacent(const V8 &a) {
	V8 o;
	const uint32x4_t z = vdupq_n_u32(0);
	for (int i = 0; i < 8; i++) {
		uint32x4_t prev = i > 0 ? a.v[i - 1] : z;
		uint32x4_t next = i < 7 ? a.v[i + 1] : z;
		uint32x4_t up = vextq_u32(prev, a.v[i], 3);
		uint32x4_t dn = vextq_u32(a.v[i], next, 1);
		uint32x4_t lr = vorrq_u32(vshlq_n_u32(a.v[i], 1), vshrq_n_u32(a.v[i], 1));
		o.v[i] = vorrq_u32(vorrq_u32(lr, a.v[i]), vorrq_u32(up, dn));
	}
	return o;
}

static inline V8 andv(const V8 &a, const V8 &b) {
	V8 o;
	for (int i = 0; i < 8; i++) o.v[i] = vandq_u32(a.v[i], b.v[i]);
	return o;
}

static inline bool bit(const uint32_t *w, int pos) {
	return (w[pos >> 5] >> (pos & 31)) & 1;
}

template<int A, int B, bool Check, bool Diff = Check>
static inline void step(V8 &Path, const V8 &Mv, const V8 &T, uint32x4_t *tfm, uint32x4_t &f, uint32x4_t &df) {
	const uint32x4_t z = vdupq_n_u32(0);
	uint32x4_t n[8];
	for (int i = A; i <= B; i++) {
		uint32x4_t prev = i > 0 ? Path.v[i - 1] : z;
		uint32x4_t next = i < 7 ? Path.v[i + 1] : z;
		uint32x4_t c = Path.v[i];
		uint32x4_t up = vextq_u32(prev, c, 3);
		uint32x4_t dn = vextq_u32(c, next, 1);
		uint32x4_t lr = vorrq_u32(vshlq_n_u32(c, 1), vshrq_n_u32(c, 1));
		uint32x4_t h = Check ? vorrq_u32(vorrq_u32(lr, c), vorrq_u32(up, dn)) : vorrq_u32(lr, vorrq_u32(up, dn));
		if (Check) {
			uint32x4_t t = vandq_u32(h, T.v[i]);
			tfm[i] = t;
			f = vorrq_u32(f, t);
		}
		n[i] = vandq_u32(h, Mv.v[i]);
		if (Diff) df = vorrq_u32(df, veorq_u32(n[i], c));
	}
	for (int i = A; i <= B; i++) Path.v[i] = n[i];
}

#define CASES C(0,0) C(0,1) C(0,2) C(0,3) C(0,4) C(0,5) C(0,6) C(0,7) C(1,1) C(1,2) C(1,3) C(1,4) C(1,5) C(1,6) C(1,7) C(2,2) C(2,3) C(2,4) C(2,5) C(2,6) C(2,7) C(3,3) C(3,4) C(3,5) C(3,6) C(3,7) C(4,4) C(4,5) C(4,6) C(4,7) C(5,5) C(5,6) C(5,7) C(6,6) C(6,7) C(7,7)

static inline int win(int r, int d) {
	int lo = r - d, hi = r + d;
	return ((lo < 0 ? 0 : lo) >> 2) * 8 + ((hi > 31 ? 31 : hi) >> 2);
}

template<int A, int B>
static inline int run1(V8 &Path, const V8 &Mv, const V8 &T, uint32x4_t *tfm, int &dist, int r0) {
	for (;;) {
		dist++;
		uint32x4_t f = vdupq_n_u32(0), df = vdupq_n_u32(0);
		step<A, B, true, true>(Path, Mv, T, tfm, f, df);
		if (vmaxvq_u32(f)) return 1;
		if ((dist & 3) == 0 && !vmaxvq_u32(df)) return 2;
		if (win(r0, dist + 1) != A * 8 + B) return 0;
	}
}

static inline int win2(int rt, int r0, int j, int dist) {
	int lo = rt - j, hi = rt + j;
	int lo2 = r0 - (dist - j), hi2 = r0 + (dist - j);
	lo = lo > lo2 ? lo : lo2;
	hi = hi < hi2 ? hi : hi2;
	return ((lo < 0 ? 0 : lo) >> 2) * 8 + ((hi > 31 ? 31 : hi) >> 2);
}

template<int A, int B>
static inline void run2(V8 &Path, const V8 &Mv, int &j, int dist, int rt, int r0) {
	uint32x4_t f, df;
	for (;;) {
		step<A, B, false>(Path, Mv, Mv, nullptr, f, df);
		j++;
		if (j > dist - 2) return;
		if (win2(rt, r0, j, dist) != A * 8 + B) return;
	}
}

static int find_move(const Map &M, const Map &Enemies, int pos) {
	V8 Mv = load(M);
	V8 T = andv(adjacent(load(Enemies)), Mv);
	V8 Path = single(pos);
	uint32x4_t tfm[8];
	const int r0 = pos >> 5;
	int dist = 0;
	int va;
	for (;;) {
		int w = win(r0, dist + 1);
		va = w >> 3;
		int st;
		switch (w) {
#undef C
#define C(a, b) case a * 8 + b: st = run1<a, b>(Path, Mv, T, tfm, dist, r0); break;
		CASES
		default: __builtin_unreachable();
		}
		if (st == 1) break;
		if (st == 2) return -1;
	}

	int t = -1;
	for (int i = va; ; i++) {
		uint64x2_t q = vreinterpretq_u64_u32(tfm[i]);
		uint64_t q0 = vgetq_lane_u64(q, 0), q1 = vgetq_lane_u64(q, 1);
		if (q0) { t = i * 128 + __builtin_ctzll(q0); break; }
		if (q1) { t = i * 128 + 64 + __builtin_ctzll(q1); break; }
	}

	Path = single(t);
	const int rt = t >> 5;
	for (int j = 1; j <= dist - 2; ) {
		switch (win2(rt, r0, j, dist)) {
#undef C
#define C(a, b) case a * 8 + b: run2<a, b>(Path, Mv, j, dist, rt, r0); break;
		CASES
		default: __builtin_unreachable();
		}
	}
	alignas(16) uint32_t P[40];
	for (int i = 0; i < 8; i++) vst1q_u32(P + 4 + 4 * i, Path.v[i]);
	P[3] = P[36] = 0;
	const uint32_t *Pr = P + 4;
	for (int off : { -32, -1, 1, 32 }) {
		int n = pos + off;
		if (!bit(M.r, n)) continue;
		if (bit(Pr, n) | bit(Pr, n - 32) | bit(Pr, n - 1) | bit(Pr, n + 1) | bit(Pr, n + 32)) return n;
	}
	return -1;
}

struct State {
	Map M, E, G;
	int total_hp, rounds, nE, nG;
	uint8_t hp[1024];
};

struct Snapshot {
	State s;
	int max_hits;
};

template<bool Part2, bool Record>
static int solve(State &st, int elf_power, std::vector<Snapshot> *snaps) {
	Map &M = st.M, &E = st.E, &G = st.G;
	uint8_t *hp = st.hp;
	int total_hp = st.total_hp;
	int nE = st.nE, nG = st.nG;
	bool recording = Record;

	alignas(16) uint32_t blocked[1024];
	memset(blocked, 0, sizeof(blocked));
	uint32_t ver = 1;

	for (int rounds = st.rounds; ; rounds++) {
		if (Record && recording) {
			int mh = 0;
			for (int y = 0; y < 32; y++) {
				for (uint32_t m = G.r[y]; m; m &= m - 1) {
					int p = y * 32 + __builtin_ctz(m);
					int k = (200 - hp[p]) / 3;
					mh = k > mh ? k : mh;
				}
			}
			snaps->emplace_back();
			Snapshot &sn = snaps->back();
			sn.s = st;
			sn.s.M = M; sn.s.E = E; sn.s.G = G;
			memcpy(sn.s.hp, hp, 1024);
			sn.s.total_hp = total_hp;
			sn.s.rounds = rounds;
			sn.s.nE = nE; sn.s.nG = nG;
			sn.max_hits = mh;
		}

		uint32_t order[32];
		for (int y = 0; y < 32; y++) order[y] = E.r[y] | G.r[y];

		for (int y = 0; y < 32; y++) {
			while (order[y]) {
				int pos = y * 32 + __builtin_ctz(order[y]);
				order[y] &= order[y] - 1;

				bool elf = E.test(pos);
				Map &Friends = elf ? E : G;
				Map &Enemies = elf ? G : E;

				if ((elf ? nG : nE) == 0) {
					return rounds * total_hp;
				}

				bool adj_enemy = Enemies.test(pos - 32) | Enemies.test(pos - 1) |
				                 Enemies.test(pos + 1) | Enemies.test(pos + 32);
				if (!adj_enemy) {
					if (blocked[pos] == ver) continue;

					int pos_new = find_move(M, Enemies, pos);
					if (pos_new < 0) {
						blocked[pos] = ver;
						continue;
					}
					ver++;
					hp[pos_new] = hp[pos];
					M.toggle(pos); Friends.toggle(pos);
					pos = pos_new;
					M.toggle(pos); Friends.toggle(pos);
				}

				int v_pos = -1;
				int best_hp = 0x100;
				for (int offset : { -32, -1, 1, 32 }) {
					int q = pos + offset;
					if (Enemies.test(q) && hp[q] < best_hp) {
						v_pos = q;
						best_hp = hp[q];
					}
				}
				if (v_pos < 0) continue;

				int damage = elf ? elf_power : 3;
				if (damage < hp[v_pos]) {
					total_hp -= damage;
					hp[v_pos] -= damage;
				} else {
					if (Part2 && !elf) {
						return 0;
					}
					recording = false;
					total_hp -= hp[v_pos];
					hp[v_pos] = 0;
					ver++;
					M.toggle(v_pos); Enemies.toggle(v_pos);
					if (elf) nG--; else nE--;
					order[v_pos >> 5] &= ~(1u << (v_pos & 31));
				}
			}
		}
	}
}

}

void day15(input_t input) {
	if (input.len != 1056) {
		fprintf(stderr, "Day 15: Expected 1056 bytes of input, got %lu\n", input.len);
		abort();
	}

	State init;
	init.M.clear(); init.E.clear(); init.G.clear();
	memset(init.hp, 0, sizeof(init.hp));
	init.nE = init.nG = 0;
	init.rounds = 0;

	const uint8x16_t bitw = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
	auto mask32 = [&](uint8x16_t a, uint8x16_t b) -> uint32_t {
		uint8x16_t x = vandq_u8(a, bitw), y = vandq_u8(b, bitw);
		uint8x16_t p = vpaddq_u8(x, y);
		p = vpaddq_u8(p, p);
		p = vpaddq_u8(p, p);
		return vgetq_lane_u32(vreinterpretq_u32_u8(p), 0);
	};

	const char *s = input.s;
	for (int y = 0; y < 32; y++, s += 33) {
		uint8x16_t a = vld1q_u8((const uint8_t *) s);
		uint8x16_t b = vld1q_u8((const uint8_t *) s + 16);
		init.M.r[y] = mask32(vceqq_u8(a, vdupq_n_u8('.')), vceqq_u8(b, vdupq_n_u8('.')));
		init.E.r[y] = mask32(vceqq_u8(a, vdupq_n_u8('E')), vceqq_u8(b, vdupq_n_u8('E')));
		init.G.r[y] = mask32(vceqq_u8(a, vdupq_n_u8('G')), vceqq_u8(b, vdupq_n_u8('G')));
		uint32_t u = init.E.r[y] | init.G.r[y];
		init.nE += __builtin_popcount(init.E.r[y]);
		init.nG += __builtin_popcount(init.G.r[y]);
		for (; u; u &= u - 1) init.hp[y * 32 + __builtin_ctz(u)] = 200;
	}
	init.total_hp = 200 * (init.nE + init.nG);

	std::vector<Snapshot> snaps;
	snaps.reserve(128);

	State st = init;
	int part1 = solve<false, true>(st, 3, &snaps);

	int part2 = 0;
	int prev_hits = -1;
	for (int elf_power = 4; !part2; elf_power++) {
		int hits = (200 + elf_power - 1) / elf_power;
		if (hits == prev_hits) continue;
		prev_hits = hits;

		int idx = -1;
		for (int i = (int) snaps.size() - 1; i >= 0; i--) {
			if (snaps[i].max_hits < hits) { idx = i; break; }
		}
		if (idx < 0) {
			st = init;
		} else {
			st = snaps[idx].s;
			int total = 0;
			for (int y = 0; y < 32; y++) {
				for (uint32_t m = st.G.r[y]; m; m &= m - 1) {
					int p = y * 32 + __builtin_ctz(m);
					int k = (200 - st.hp[p]) / 3;
					st.hp[p] = 200 - elf_power * k;
				}
				for (uint32_t m = st.E.r[y] | st.G.r[y]; m; m &= m - 1) {
					total += st.hp[y * 32 + __builtin_ctz(m)];
				}
			}
			st.total_hp = total;
		}
		part2 = solve<true, false>(st, elf_power, nullptr);
	}
	printf("Day 15 Part 1: %d\nDay 15 Part 2: %d\n", part1, part2);
}
