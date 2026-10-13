#include "advent2018.h"
#include <arm_neon.h>

namespace {

constexpr size_t MAXN = 4096;

struct Glyph {
	uint64_t bits;
	char c;
};

constexpr Glyph glyph_table[] = {
	{ 0x861861fe186148c, 'A' },
	{ 0x7e186185f86185f, 'B' },
	{ 0x7a104104104185e, 'C' },
	{ 0xfc104105f04107f, 'E' },
	{ 0x04104105f04107f, 'F' },
	{ 0xbb1861e4104185e, 'G' },
	{ 0x86186187f861861, 'H' },
	{ 0x391450410410438, 'J' },
	{ 0x8512450c3149461, 'K' },
	{ 0xfc1041041041041, 'L' },
	{ 0x871c69a659638e1, 'N' },
	{ 0x04104105f86185f, 'P' },
	{ 0x86145125f86185f, 'R' },
	{ 0x86149230c492861, 'X' },
	{ 0xfc104210842083f, 'Z' },
};

static inline char recognize(uint64_t n) {
	for (auto &g : glyph_table) {
		if (g.bits == n) return g.c;
	}
	return 0;
}

struct Layout {
	size_t L, c1, c2, c3, c4, lo;
};

template <int R>
__attribute__((always_inline)) static inline uint8x16_t gather(const uint8_t *p, uint8x16_t idx) {
	if constexpr (R == 2) {
		return vqtbl2q_u8(vld1q_u8_x2(p), idx);
	} else if constexpr (R == 3) {
		return vqtbl3q_u8(vld1q_u8_x3(p), idx);
	} else {
		return vqtbl4q_u8(vld1q_u8_x4(p), idx);
	}
}

template <int R>
static bool parse_lines(const char *s, size_t n, const Layout &lay, const uint8_t *idx, int32x4_t *P, int64_t &kmin_out, int64_t &kmax_out) {
	int64_t kmin = INT64_MAX, kmax = INT64_MIN;
	const uint8x16_t idxA = vld1q_u8(idx), idxB = vld1q_u8(idx + 16);
	const uint8x16_t k30 = vdupq_n_u8(0x30), kminus = vdupq_n_u8('-');
	const uint8x16_t w8 = { 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1 };
	const uint8x16_t whi = { 100, 1, 0, 0, 100, 1, 0, 0, 100, 1, 0, 0, 100, 1, 0, 0 };
	const uint8x16_t wlo = { 0, 0, 100, 1, 0, 0, 100, 1, 0, 0, 100, 1, 0, 0, 100, 1 };
	const size_t L = lay.L, c1 = lay.c1, c2 = lay.c2, c3 = lay.c3, c4 = lay.c4, off = lay.lo;
	const char *p = s;
	bool ok = true;
#pragma clang loop unroll_count(2)
	for (size_t i = 0; i < n; i++, p += L) {
		ok &= (p[c1] == ',') & (p[c2] == '>') & (p[c3] == ',') & (p[c4] == '>') & ((p[L - 1] == '\n') | (i == n - 1));
		const uint8_t *q = (const uint8_t *)p + off;
		uint8x16_t a = gather<R>(q, idxA);
		uint8x16_t b = gather<R>(q, idxB);
		uint8x16_t da = vqsubq_u8(a, k30), db = vqsubq_u8(b, k30);
		uint8x16_t pairs = vpaddq_u8(vmulq_u8(da, w8), vmulq_u8(db, w8));
		uint32x4_t hi = vdotq_u32(vdupq_n_u32(0), pairs, whi);
		uint32x4_t r = vdotq_u32(vmulq_n_u32(hi, 10000), pairs, wlo);
		uint8x16_t m = vpmaxq_u8(vceqq_u8(a, kminus), vceqq_u8(b, kminus));
		uint32x4_t neg = vtstq_u32(vreinterpretq_u32_u8(m), vreinterpretq_u32_u8(m));
		int32x4_t v = vreinterpretq_s32_u32(vsubq_u32(veorq_u32(r, neg), neg));
		P[i] = v;
		int64_t k = (int64_t)(vgetq_lane_u64(vreinterpretq_u64_s32(v), 1) ^ 0x80000000ull);
		kmin = std::min(kmin, k);
		kmax = std::max(kmax, k);
	}
	kmin_out = kmin;
	kmax_out = kmax;
	return ok;
}

static int64_t point_key(int32x4_t v) {
	return (int64_t)(vgetq_lane_u64(vreinterpretq_u64_s32(v), 1) ^ 0x80000000ull);
}

static void solve(const int32x4_t *P, size_t n, int64_t kmin, int64_t kmax) {
	int32_t dy_up = (int32_t)(kmin >> 32), y_up = (int32_t)((uint32_t)kmin ^ INT32_MIN);
	int32_t dy_down = (int32_t)(kmax >> 32), y_down = (int32_t)((uint32_t)kmax ^ INT32_MIN);

	int32_t part2 = 1 + (y_up - y_down) / (dy_down - dy_up);

	int32x4_t T = vdupq_n_s32(part2);
	int32x4_t bb = vdupq_n_s32(INT32_MAX);
	std::vector<int32x4_t> Fheap;
	int32x4_t Fstack[MAXN / 2 + 1];
	int32x4_t *F = Fstack;
	if (n > MAXN) {
		Fheap.resize(n / 2 + 1);
		F = Fheap.data();
	}
	size_t i = 0;
	for (; i + 2 <= n; i += 2) {
		int32x4_t f = vmlaq_s32(vuzp1q_s32(P[i], P[i + 1]), vuzp2q_s32(P[i], P[i + 1]), T);
		F[i / 2] = f;
		bb = vminq_s32(bb, f);
	}
	if (i < n) {
		int32x4_t f = vmlaq_s32(vuzp1q_s32(P[i], P[i]), vuzp2q_s32(P[i], P[i]), T);
		F[i / 2] = f;
		bb = vminq_s32(bb, f);
	}
	int32_t bb_left = std::min(vgetq_lane_s32(bb, 0), vgetq_lane_s32(bb, 2));
	int32_t bb_up = std::min(vgetq_lane_s32(bb, 1), vgetq_lane_s32(bb, 3));
	const int32_t *Fs = (const int32_t *)F;

	uint64_t text[4][8] = { };
	for (size_t i = 0; i < n; i++) {
		uint32_t x = (uint32_t)(Fs[2 * i] - bb_left);
		uint32_t y = (uint32_t)(Fs[2 * i + 1] - bb_up);
		text[i & 3][(x >> 3) & 7] |= uint64_t(1) << ((6 * y + (x & 7)) & 63);
	}

	char part1[9];
	for (int i = 0; i < 8; i++) part1[i] = recognize(text[0][i] | text[1][i] | text[2][i] | text[3][i]);
	part1[8] = 0;

	printf("Day 10 Part 1: %s\nDay 10 Part 2: %u\n", part1, part2);
}

static void generic(input_t input) {
	std::vector<int32x4_t> V;
	int32_t val[4];
	for (int32_t n = 0, neg = 0, have = 0, i = 0; input.len--; input.s++) {
		uint8_t c = *input.s - '0';
		if (c < 10) {
			n = 10 * n + c;
			have = 1;
			continue;
		}
		neg |= (*input.s == '-');
		if (have) {
			val[i++] = n - neg ^ -neg;
			n = neg = have = 0;
			if (i == 4) {
				int32_t o[4] = { val[0], val[2], val[1], val[3] };
				V.push_back(vld1q_s32(o));
				i = 0;
			}
		}
	}
	int64_t kmin = INT64_MAX, kmax = INT64_MIN;
	for (auto &v : V) {
		kmin = std::min(kmin, point_key(v));
		kmax = std::max(kmax, point_key(v));
	}
	solve(V.data(), V.size(), kmin, kmax);
}

}

void day10(input_t input) {
	const char *s = input.s;
	size_t len = input.len;
	const char *nl = (const char *)memchr(s, '\n', len);
	if (!nl || len < 24 || memcmp(s, "position=<", 10) != 0) return generic(input);
	size_t L = nl - s + 1;
	const char *c1p = (const char *)memchr(s, ',', L);
	if (!c1p) return generic(input);
	size_t c1 = c1p - s;
	const char *c2p = (const char *)memchr(c1p, '>', L - c1);
	if (!c2p) return generic(input);
	size_t c2 = c2p - s;
	if (c2 + 12 >= L || memcmp(c2p, "> velocity=<", 12) != 0) return generic(input);
	const char *c3p = (const char *)memchr(c2p, ',', L - c2);
	if (!c3p) return generic(input);
	size_t c3 = c3p - s;
	size_t c4 = L - 2;
	size_t n = (len + 1) / L;
	size_t w1 = c1 - 10, w2 = c2 - c1 - 1, w3 = c3 - c2 - 12, w4 = c4 - c3 - 1;
	if ((len + 1) % L > 1 || n > MAXN || L > 64 || s[c4] != '>' ||
	    w1 > 8 || w2 > 8 || w3 > 8 || w4 > 8)
		return generic(input);

	size_t lo = c1 - w1, hi = c4;
	unsigned R = (unsigned)((hi - lo + 15) / 16);
	uint8_t idx[32];
	const size_t ends[4] = { c1, c3, c2, c4 }, widths[4] = { w1, w3, w2, w4 };
	for (int f = 0; f < 4; f++) {
		for (int b = 0; b < 8; b++) {
			int pos = (int)ends[f] - 8 + b - (int)lo;
			idx[f * 8 + b] = (b >= 8 - (int)widths[f]) ? (uint8_t)pos : 0xff;
		}
	}
	Layout lay = { L, c1, c2, c3, c4, lo };
	int32x4_t P[MAXN];
	bool ok;
	int64_t kmin, kmax;
	if (R <= 2) ok = parse_lines<2>(s, n, lay, idx, P, kmin, kmax);
	else if (R <= 3) ok = parse_lines<3>(s, n, lay, idx, P, kmin, kmax);
	else ok = parse_lines<4>(s, n, lay, idx, P, kmin, kmax);
	if (!ok) return generic(input);
	solve(P, n, kmin, kmax);
}
