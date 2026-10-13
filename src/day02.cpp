#include "advent2018.h"
#include <arm_neon.h>

namespace {

constexpr size_t LEN = 27;
constexpr uint32_t TBITS = 10;
constexpr uint32_t TSIZE = 1u << TBITS;

inline uint64_t load64(const char *p) {
	uint64_t x;
	memcpy(&x, p, 8);
	return x;
}

inline int single_diff(const char *a, const char *b) {
	uint8x16_t eq0 = vceqq_u8(vld1q_u8((const uint8_t *)a), vld1q_u8((const uint8_t *)b));
	uint8x16_t eq1 = vceqq_u8(vld1q_u8((const uint8_t *)a + 16), vld1q_u8((const uint8_t *)b + 16));
	uint64_t m0 = ~vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(eq0), 4)), 0);
	uint64_t m1 = ~vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(eq1), 4)), 0) & 0xffffffffffull;
	uint64_t m = m0 ? m0 : m1;
	if ((m0 && m1) || !m) return -1;
	int bit = __builtin_ctzll(m) & ~3;
	if (m != (0xfull << bit)) return -1;
	return (bit >> 2) + (m0 ? 0 : 16);
}

inline void count_word(const char *s, int &n_two, int &n_three) {
	uint32_t s1 = 0, s2 = 0, s3 = 0, s4 = 0;
	for (int k = 0; k < 26; k++) {
		uint32_t b = 1u << (s[k] & 31);
		s4 |= s3 & b;
		s3 |= s2 & b;
		s2 |= s1 & b;
		s1 |= b;
	}
	n_two += (s2 & ~s3) != 0;
	n_three += (s3 & ~s4) != 0;
}

struct Matcher {
	static constexpr uint64_t L1 = 0x0001000100010001ull, H = 0x8000800080008000ull;
	const char *base;
	const char *ans = nullptr;
	int diff_idx = 0;
	alignas(64) uint64_t tags[TSIZE];
	uint16_t idxs[TSIZE * 4];

	explicit Matcher(const char *base) : base(base) {
		memset(tags, 0, sizeof(tags));
	}

	bool insert_word(size_t i) {
		static constexpr int OFF[4] = { 0, 7, 13, 20 };
		static constexpr uint64_t MASK[4] = { 0xffffffffffffffull, 0xffffffffffffull, 0xffffffffffffffull, 0xffffffffffffull };
		static constexpr uint64_t MUL[4] = { 0x9e3779b97f4a7c15ull, 0xc2b2ae3d27d4eb4full, 0x165667b19e3779f9ull, 0xd6e8feb86659fd93ull };
		const char *s = base + i * LEN;
		uint64_t hp[4], tot = 0;
		for (int k = 0; k < 4; k++) {
			hp[k] = (load64(s + OFF[k]) & MASK[k]) * MUL[k];
			tot += hp[k];
		}
		for (int k = 0; k < 4; k++) {
			uint64_t h = (tot - hp[k]) * 0xff51afd7ed558ccdull;
			uint32_t bucket = (uint32_t)(h >> (64 - TBITS));
			uint64_t tag = ((h >> 16) & 0xffff) | 0x8000u;
			uint64_t tagv = tag * L1;
			for (;;) {
				uint64_t bv = tags[bucket];
				uint64_t x = bv ^ tagv;
				uint64_t tm = (x - L1) & ~x & H;
				uint64_t em = ~bv & H;
				if (__builtin_expect(tm != 0, 0)) {
					do {
						int lane = __builtin_ctzll(tm) >> 4;
						const char *t = base + idxs[bucket * 4 + lane] * LEN;
						int d = single_diff(t, s);
						if (d >= 0) {
							ans = t;
							diff_idx = d;
							return true;
						}
						tm &= tm - 1;
					} while (tm);
				}
				if (__builtin_expect(em != 0, 1)) {
					int lane = __builtin_ctzll(em) >> 4;
					tags[bucket] = bv | (tag << (lane * 16));
					idxs[bucket * 4 + lane] = (uint16_t)i;
					break;
				}
				bucket = (bucket + 1) & (TSIZE - 1);
			}
		}
		return false;
	}
};

}

void day02(input_t input) {
	const char *base = input.s;
	size_t n = input.len >= 0 ? (size_t)input.len / LEN : 0;

	Matcher mt(base);
	bool found = false;

	uint32x4_t acc2 = vdupq_n_u32(0), acc3 = vdupq_n_u32(0);
	const uint32x4_t one = vdupq_n_u32(1);
	const uint8x16_t k97 = vdupq_n_u8(97);
	size_t i = 0;
	for (; i + 4 <= n; i += 4) {
		const uint8_t *p = (const uint8_t *)base + i * LEN;
		uint32x4_t r[4][2];
		for (int w = 0; w < 4; w++) {
			r[w][0] = vreinterpretq_u32_u8(vsubq_u8(vld1q_u8(p + w * LEN), k97));
			r[w][1] = vreinterpretq_u32_u8(vsubq_u8(vld1q_u8(p + w * LEN + 16), k97));
		}
		uint32x4_t col[8];
		for (int h = 0; h < 2; h++) {
			uint32x4_t t0 = vtrn1q_u32(r[0][h], r[1][h]);
			uint32x4_t t1 = vtrn2q_u32(r[0][h], r[1][h]);
			uint32x4_t t2 = vtrn1q_u32(r[2][h], r[3][h]);
			uint32x4_t t3 = vtrn2q_u32(r[2][h], r[3][h]);
			col[h * 4 + 0] = vreinterpretq_u32_u64(vtrn1q_u64(vreinterpretq_u64_u32(t0), vreinterpretq_u64_u32(t2)));
			col[h * 4 + 1] = vreinterpretq_u32_u64(vtrn1q_u64(vreinterpretq_u64_u32(t1), vreinterpretq_u64_u32(t3)));
			col[h * 4 + 2] = vreinterpretq_u32_u64(vtrn2q_u64(vreinterpretq_u64_u32(t0), vreinterpretq_u64_u32(t2)));
			col[h * 4 + 3] = vreinterpretq_u32_u64(vtrn2q_u64(vreinterpretq_u64_u32(t1), vreinterpretq_u64_u32(t3)));
		}
		uint32x4_t s1 = vdupq_n_u32(0), s2 = s1, s3 = s1, s4 = s1;
		auto step = [&](uint32x4_t x) {
			uint32x4_t b = vshlq_u32(one, vreinterpretq_s32_u32(x));
			s4 = vbslq_u32(b, s3, s4);
			s3 = vbslq_u32(b, s2, s3);
			s2 = vbslq_u32(b, s1, s2);
			s1 = vorrq_u32(s1, b);
		};
		for (int c = 0; c < 7; c++) {
			uint32x4_t x = col[c];
			step(x);
			step(vshrq_n_u32(x, 8));
			if (c < 6) {
				step(vshrq_n_u32(x, 16));
				step(vshrq_n_u32(x, 24));
			}
		}
		uint32x4_t t2 = vbicq_u32(s2, s3), t3 = vbicq_u32(s3, s4);
		acc2 = vsubq_u32(acc2, vtstq_u32(t2, t2));
		acc3 = vsubq_u32(acc3, vtstq_u32(t3, t3));
		if (!found) {
			for (size_t w = i; w < i + 4; w++) {
				if (mt.insert_word(w)) {
					found = true;
					break;
				}
			}
		}
	}
	int n_two = vaddvq_u32(acc2), n_three = vaddvq_u32(acc3);
	for (; i < n; i++) {
		count_word(base + i * LEN, n_two, n_three);
		if (!found && mt.insert_word(i)) found = true;
	}
	int part1 = n_two * n_three;

	if (found) {
		const char *ans = mt.ans;
		int diff_idx = mt.diff_idx;
		printf("Day 02 Part 1: %u\nDay 02 Part 2: %.*s%.*s\n", part1, diff_idx, ans, 25 - diff_idx, ans + diff_idx + 1);
	}
}
