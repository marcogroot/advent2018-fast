#include "advent2018.h"
#include <arm_neon.h>

namespace {

typedef unsigned __int128 u128;

constexpr int Xmax = 80;
constexpr int Ymax_margin = 8;
constexpr uint32_t MOD = 20183;

struct Row {
	u128 avail[3];
};

struct Front {
	u128 f[3];
};

inline uint64_t compress_even(uint64_t x) {
	x &= 0x5555555555555555ull;
	x = (x | (x >> 1)) & 0x3333333333333333ull;
	x = (x | (x >> 2)) & 0x0F0F0F0F0F0F0F0Full;
	x = (x | (x >> 4)) & 0x00FF00FF00FF00FFull;
	x = (x | (x >> 8)) & 0x0000FFFF0000FFFFull;
	x = (x | (x >> 16)) & 0x00000000FFFFFFFFull;
	return x;
}

inline u128 split(u128 m) {
	uint64_t lo = (uint64_t)m, hi = (uint64_t)(m >> 64);
	uint64_t even = compress_even(lo) | (compress_even(hi) << 32);
	uint64_t odd = compress_even(lo >> 1) | (compress_even(hi >> 1) << 32);
	return ((u128)odd << 64) | even;
}

constexpr int KV = 6;
constexpr int KR = 4 * KV;
constexpr int NS = Xmax + KR - 1;

inline uint32x4_t modv(uint32x4_t p) {
	constexpr int32_t magic = (int32_t)(((1ull << 44) + MOD - 1) / MOD);
	uint32x4_t q = vshrq_n_u32(vreinterpretq_u32_s32(vqdmulhq_n_s32(vreinterpretq_s32_u32(p), magic)), 13);
	return vmlsq_n_u32(p, q, MOD);
}

struct GenStrip {
	uint32x4_t L[KV], aw[KV], an[KV];
	alignas(16) uint32_t tsel[KR];
	uint32x4_t dep, dmod;
	uint32_t *r;
	int sT;
	alignas(16) uint32_t wb[4][KR], nb[4][KR];

	template <int S>
	__attribute__((always_inline)) inline void step() {
		constexpr int J = S & 31;
		constexpr int W = S >> 5;
		const uint32x4_t m3 = vdupq_n_u32(0x55555556u);
		uint32x4_t o[KV];
		uint32x4_t r_s = vdupq_n_u32(r[S]);
		for (int j = 0; j < KV; j++) {
			uint32x4_t prod = vmulq_u32(L[j], m3);
			aw[j] = vsliq_n_u32(aw[j], vshrq_n_u32(prod, 30), J);
			an[j] = vsliq_n_u32(an[j], vshrq_n_u32(prod, 31), J);
			uint32x4_t up = vextq_u32(j == 0 ? r_s : L[j - 1], L[j], 3);
			o[j] = modv(vmlaq_u32(dep, L[j], up));
		}
		if constexpr (J == 31 || S == NS - 1) {
			for (int j = 0; j < KV; j++) {
				vst1q_u32(wb[W] + 4 * j, aw[j]);
				vst1q_u32(nb[W] + 4 * j, an[j]);
			}
		}
		if constexpr (S < KR - 1) {
			const uint32x4_t sv = vdupq_n_u32(S);
			const uint32x4_t kv = { 0, 1, 2, 3 };
			for (int j = 0; j < KV; j++) {
				o[j] = vbslq_u32(vcleq_u32(vaddq_u32(kv, vdupq_n_u32(4 * j)), sv), o[j], L[j]);
			}
		}
		if (S == sT) {
			for (int j = 0; j < KV; j++) {
				o[j] = vbslq_u32(vld1q_u32(tsel + 4 * j), dmod, o[j]);
			}
		}
		if constexpr (S >= KR - 1) {
			r[S - (KR - 1)] = vgetq_lane_u32(o[KV - 1], 3);
		}
		for (int j = 0; j < KV; j++) {
			L[j] = o[j];
		}
	}

	template <int... I>
	__attribute__((always_inline)) inline void run(std::integer_sequence<int, I...>) {
		(step<I>(), ...);
	}
};

}

void day22(input_t input) {
	std::array<uint32_t, 3> A = { 0, 0, 0 };
	for (uint32_t n = 0, i = 0; input.len-- && i < 3; input.s++) {
		uint8_t c = *input.s - '0';
		if (c < 10) {
			n = 10 * n + c;
		} else if (n) {
			std::swap(A[i++], n);
		}
	}

	const uint32_t depth = (uint16_t)A[0];
	const int Tx = (uint8_t)A[1];
	const int Ty = (uint16_t)A[2];
	const int Ymax = Ty + Ymax_margin;
	const int Tx1 = (uint16_t)(Tx - 1);

	std::vector<Row> rows(Ymax);
	const u128 valid = ((u128)1 << Xmax) - 1;
	const u128 tmask = ((u128)2 << Tx) - 1;
	uint32_t part1 = 0;
	const u128 valid2 = ((u128)0xFFFFFFFFFFull << 64) | 0xFFFFFFFFFFull;
	auto set_row = [&](int yy, u128 W, u128 N) {
		W &= valid;
		N &= valid;
		if (yy <= Ty) {
			u128 wt = W & tmask, nt = N & tmask;
			part1 += __builtin_popcountll((uint64_t)wt) + __builtin_popcountll((uint64_t)(wt >> 64));
			part1 += 2 * (__builtin_popcountll((uint64_t)nt) + __builtin_popcountll((uint64_t)(nt >> 64)));
		}
		W = split(W);
		N = split(N);
		Row &R = rows[yy];
		R.avail[0] = valid2 & ~W;
		R.avail[1] = valid2 & ~N;
		R.avail[2] = W | N;
	};

	alignas(16) uint32_t r[NS + 1] = {};
	{
		uint32_t prev = depth;
		u128 W = 0, N = 0;
		for (int x = 0; x < Xmax; x++) {
			uint32_t t = (0x55555556u * prev) >> 30;
			W |= (u128)(t == 1) << x;
			N |= (u128)(t == 2) << x;
			prev += 16807;
			prev -= -(prev >= MOD) & MOD;
			r[x] = prev;
		}
		set_row(0, W, N);
	}

	GenStrip g;
	g.dep = vdupq_n_u32(depth);
	g.dmod = vdupq_n_u32(depth % MOD);
	g.r = r;

	uint32_t x0 = depth + 7905;
	for (int y = 1; y < Ymax; y += KR) {
		alignas(16) uint32_t init[KR];
		for (int k = 0; k < KR; k++) {
			init[k] = x0;
			x0 += 7905;
			x0 -= -(x0 >= MOD) & MOD;
		}
		for (int j = 0; j < KV; j++) {
			g.L[j] = vld1q_u32(init + 4 * j);
		}
		int kT = Ty - y;
		g.sT = ((unsigned)kT < (unsigned)KR && Tx1 < Xmax) ? Tx1 + kT : -1;
		if (g.sT >= 0) {
			memset(g.tsel, 0, sizeof g.tsel);
			g.tsel[kT] = ~0u;
		}
		g.run(std::make_integer_sequence<int, NS>{});
		int kmax = std::min(KR, Ymax - y);
		for (int k = 0; k < kmax; k++) {
			u128 mw = (u128)g.wb[0][k] | ((u128)g.wb[1][k] << 32) | ((u128)g.wb[2][k] << 64) | ((u128)g.wb[3][k] << 96);
			u128 mn = (u128)g.nb[0][k] | ((u128)g.nb[1][k] << 32) | ((u128)g.nb[2][k] << 64) | ((u128)g.nb[3][k] << 96);
			set_row(y + k, mw >> k, mn >> k);
		}
	}

	const int H = Ymax + 2;
	std::vector<Front> ring(8 * H);
	auto F = [&](int slot) { return ring.data() + slot * H + 1; };
	constexpr int EMPTY = 1 << 20;
	int lo[8], hi[8];
	for (int i = 0; i < 8; i++) {
		lo[i] = EMPTY;
		hi[i] = -EMPTY;
	}

	rows[0].avail[0] &= ~(u128)1;
	F(0)[0].f[0] = 1;
	lo[0] = hi[0] = 0;

	const u128 tbit = (u128)1 << ((Tx >> 1) + 64 * (Tx & 1));
	const bool wet_target = !(rows[Ty].avail[0] & tbit);
	const int tool_a = wet_target ? 1 : 0;
	const int tool_b = wet_target ? 2 : 0;
	const uint32_t penalty = wet_target ? 7 : 0;
	uint32_t part2 = 0;
	if (!(Tx == 0 && Ty == 0)) {
		uint64_t *__restrict RW = reinterpret_cast<uint64_t *>(rows.data());
		const uint64x2_t z = vdupq_n_u64(0);
		const int tw_a = 6 * Ty + 2 * tool_a + (Tx & 1);
		const int tw_b = 6 * Ty + 2 * tool_b + (Tx & 1);
		const uint64_t tw_bit = 1ull << (Tx >> 1);
		const int64x2_t shv = { 1, -1 };
		auto horiz = [&](uint64x2_t c) {
			uint64x2_t sw = vextq_u64(c, c, 1);
			return vorrq_u64(sw, vshlq_u64(sw, shv));
		};
		auto clear_rows = [&](uint64_t *f, int from, int to) {
			for (int i = from; i <= to; i++) {
				vst1q_u64(f + 6 * i, z);
				vst1q_u64(f + 6 * i + 2, z);
				vst1q_u64(f + 6 * i + 4, z);
			}
		};
		auto clear_outside = [&](uint64_t *f, int olo, int ohi, int A, int B) {
			clear_rows(f, olo, std::min(ohi, A - 1));
			clear_rows(f, std::max(olo, B + 1), ohi);
		};
		auto shrink = [&](const uint64_t *f, int A, int B, int &olo, int &ohi) {
			auto row_nz = [&](int i) {
				const uint64_t *r = f + 6 * i;
				return (r[0] | r[1] | r[2] | r[3] | r[4] | r[5]) != 0;
			};
			while (A <= B && !row_nz(A)) {
				A++;
			}
			while (B >= A && !row_nz(B)) {
				B--;
			}
			if (A <= B) {
				olo = A;
				ohi = B;
			} else {
				olo = EMPTY;
				ohi = -EMPTY;
			}
		};
		auto ldS = [&](const uint64_t *f) {
			return vorrq_u64(vorrq_u64(vld1q_u64(f), vld1q_u64(f + 2)), vld1q_u64(f + 4));
		};
		for (uint32_t t = 0;; t += 2) {
			int s0 = t & 7, s1 = (t + 1) & 7, s2 = (t + 2) & 7, s3 = (t + 3) & 7;
			const uint64_t *__restrict F0 = reinterpret_cast<const uint64_t *>(F(s0));
			uint64_t *__restrict F1 = reinterpret_cast<uint64_t *>(F(s1));
			uint64_t *__restrict F2 = reinterpret_cast<uint64_t *>(F(s2));
			const uint64_t *__restrict F3 = reinterpret_cast<const uint64_t *>(F(s3));
			int A = std::min(std::min(lo[s0] - 2, lo[s2] - 1), lo[s3]);
			int B = std::max(std::max(hi[s0] + 2, hi[s2] + 1), hi[s3]);
			A = std::max(A, 0);
			B = std::min(B, Ymax - 1);
			if (A > B) {
				clear_rows(F1, lo[s1], hi[s1]);
				clear_rows(F2, lo[s2], hi[s2]);
				lo[s1] = lo[s2] = EMPTY;
				hi[s1] = hi[s2] = -EMPTY;
				bool alive = false;
				for (int k = 0; k < 8; k++) {
					alive |= lo[k] <= hi[k];
				}
				if (!alive) {
					break;
				}
				continue;
			}
			clear_outside(F1, lo[s1], hi[s1], A, B);
			int olo2 = lo[s2], ohi2 = hi[s2];

			uint64x2_t p0 = vld1q_u64(F0 + 6 * (A - 1)), p1 = vld1q_u64(F0 + 6 * (A - 1) + 2), p2 = vld1q_u64(F0 + 6 * (A - 1) + 4);
			uint64x2_t c0 = vld1q_u64(F0 + 6 * A), c1 = vld1q_u64(F0 + 6 * A + 2), c2 = vld1q_u64(F0 + 6 * A + 4);
			uint64x2_t gp0 = z, gp1 = z, gp2 = z;
			uint64x2_t gc0, gc1, gc2, H0, H1, H2;
			{
				const uint64_t *fn = F0 + 6 * (A + 1);
				uint64x2_t n0 = vld1q_u64(fn), n1 = vld1q_u64(fn + 2), n2 = vld1q_u64(fn + 4);
				uint64x2_t S = ldS(F2 + 6 * A);
				uint64_t *R = RW + 6 * A;
				uint64x2_t A0 = vld1q_u64(R), A1 = vld1q_u64(R + 2), A2 = vld1q_u64(R + 4);
				gc0 = vandq_u64(vorrq_u64(vorrq_u64(vorrq_u64(p0, n0), horiz(c0)), S), A0);
				gc1 = vandq_u64(vorrq_u64(vorrq_u64(vorrq_u64(p1, n1), horiz(c1)), S), A1);
				gc2 = vandq_u64(vorrq_u64(vorrq_u64(vorrq_u64(p2, n2), horiz(c2)), S), A2);
				H0 = veorq_u64(A0, gc0); H1 = veorq_u64(A1, gc1); H2 = veorq_u64(A2, gc2);
				uint64_t *fw = F1 + 6 * A;
				vst1q_u64(fw, gc0); vst1q_u64(fw + 2, gc1); vst1q_u64(fw + 4, gc2);
				p0 = c0; p1 = c1; p2 = c2;
				c0 = n0; c1 = n1; c2 = n2;
			}
			for (int i = A + 1; i <= B + 1; i++) {
				uint64x2_t g0 = z, g1 = z, g2 = z, N0 = z, N1 = z, N2 = z;
				if (i <= B) {
					const uint64_t *fn = F0 + 6 * (i + 1);
					uint64x2_t n0 = vld1q_u64(fn), n1 = vld1q_u64(fn + 2), n2 = vld1q_u64(fn + 4);
					uint64x2_t S = ldS(F2 + 6 * i);
					uint64_t *R = RW + 6 * i;
					uint64x2_t A0 = vld1q_u64(R), A1 = vld1q_u64(R + 2), A2 = vld1q_u64(R + 4);
					g0 = vandq_u64(vorrq_u64(vorrq_u64(vorrq_u64(p0, n0), horiz(c0)), S), A0);
					g1 = vandq_u64(vorrq_u64(vorrq_u64(vorrq_u64(p1, n1), horiz(c1)), S), A1);
					g2 = vandq_u64(vorrq_u64(vorrq_u64(vorrq_u64(p2, n2), horiz(c2)), S), A2);
					N0 = veorq_u64(A0, g0); N1 = veorq_u64(A1, g1); N2 = veorq_u64(A2, g2);
					uint64_t *fw = F1 + 6 * i;
					vst1q_u64(fw, g0); vst1q_u64(fw + 2, g1); vst1q_u64(fw + 4, g2);
					p0 = c0; p1 = c1; p2 = c2;
					c0 = n0; c1 = n1; c2 = n2;
				}
				int j = i - 1;
				uint64x2_t S = ldS(F3 + 6 * j);
				uint64x2_t d0 = vandq_u64(vorrq_u64(vorrq_u64(vorrq_u64(gp0, g0), horiz(gc0)), S), H0);
				uint64x2_t d1 = vandq_u64(vorrq_u64(vorrq_u64(vorrq_u64(gp1, g1), horiz(gc1)), S), H1);
				uint64x2_t d2 = vandq_u64(vorrq_u64(vorrq_u64(vorrq_u64(gp2, g2), horiz(gc2)), S), H2);
				uint64_t *R = RW + 6 * j;
				vst1q_u64(R, veorq_u64(H0, d0));
				vst1q_u64(R + 2, veorq_u64(H1, d1));
				vst1q_u64(R + 4, veorq_u64(H2, d2));
				uint64_t *fw = F2 + 6 * j;
				vst1q_u64(fw, d0); vst1q_u64(fw + 2, d1); vst1q_u64(fw + 4, d2);
				gp0 = gc0; gp1 = gc1; gp2 = gc2;
				gc0 = g0; gc1 = g1; gc2 = g2;
				H0 = N0; H1 = N1; H2 = N2;
			}
			clear_outside(F2, olo2, ohi2, A, B);
			shrink(F1, A, B, lo[s1], hi[s1]);
			shrink(F2, A, B, lo[s2], hi[s2]);
			if (!(RW[tw_a] & tw_bit) | !(RW[tw_b] & tw_bit)) {
				bool first = ((F1[tw_a] | F1[tw_b]) & tw_bit) != 0;
				part2 = t + (first ? 1 : 2) + penalty;
				break;
			}
		}
	}

	printf("Day 22 Part 1: %u\nDay 22 Part 2: %u\n", part1, part2);
}
