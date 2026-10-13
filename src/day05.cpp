#include "advent2018.h"
#include <arm_neon.h>
#include <memory>

namespace {

constexpr size_t PAD = 64;
constexpr int LANES = 8;

inline uint64_t nibble_mask(uint8x16_t eq) {
	return vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(eq), 4)), 0);
}

inline uint8_t *merge_scalar(uint8_t *sp, const uint8_t *src, size_t L) {
	size_t m = 0;
	while (m < L && (sp[-1 - (ptrdiff_t)m] ^ src[m]) == 0x20) m++;
	sp -= m;
	for (size_t i = m; i < L; i++) sp[i - m] = src[i];
	return sp + (L - m);
}

inline void reverse_into(uint8_t *dst, const uint8_t *src, size_t n) {
	const uint8x16_t rev = {15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0};
	size_t i = 0;
	for (; i + 16 <= n; i += 16) {
		vst1q_u8(dst + n - 16 - i, vqtbl1q_u8(vld1q_u8(src + i), rev));
	}
	for (; i < n; i++) dst[n - 1 - i] = src[i];
}

}

void day05(input_t input) {
	const uint8_t *in = (const uint8_t *)input.s;
	size_t n = input.len;
	while (n && in[n - 1] < 'A') n--;

	size_t blk = n / LANES;
	size_t region = blk + (n - blk * LANES) + 2 * PAD;
	size_t total = PAD + LANES * region + 3 * (n + 2 * PAD);
	std::unique_ptr<uint8_t[]> buf(new uint8_t[total]);
	uint8_t *S = buf.get() + PAD;
	memset(buf.get(), 0, PAD + 1);

	uint8_t *sp[LANES];
	uint8_t top[LANES];
	const uint8_t *src[LANES];
	for (int j = 0; j < LANES; j++) {
		sp[j] = S + j * region;
		sp[j][0] = 0;
		top[j] = 0;
		src[j] = in + j * blk;
	}
	for (size_t i = 0; i < blk; i++) {
#pragma clang loop unroll(full)
		for (int j = 0; j < LANES; j++) {
			uint8_t c = src[j][i];
			uint8_t *p = sp[j];
			uint8_t prev = p[-1];
			p[1] = c;
			bool r = (uint8_t)(c ^ top[j]) == 0x20;
			sp[j] = r ? p - 1 : p + 1;
			top[j] = r ? prev : c;
		}
	}
	{
		uint8_t *p = sp[LANES - 1];
		uint8_t t = top[LANES - 1];
		for (size_t i = blk * LANES; i < n; i++) {
			uint8_t c = in[i];
			uint8_t prev = p[-1];
			p[1] = c;
			bool r = (uint8_t)(c ^ t) == 0x20;
			p = r ? p - 1 : p + 1;
			t = r ? prev : c;
		}
		sp[LANES - 1] = p;
	}

	uint8_t *s = sp[0] + 1;
	for (int j = 1; j < LANES; j++) {
		uint8_t *base = S + j * region + 1;
		s = merge_scalar(s, base, sp[j] + 1 - base);
	}
	size_t n2 = s - (S + 1);
	uint8_t *P = S + 1;

	uint8_t *RP = S + LANES * region + PAD;
	memset(RP - PAD, 0, PAD);
	memset(RP + n2, 0, 2 * PAD);
	memset(s, 0, PAD);
	reverse_into(RP, P, n2);

	size_t qn = n2 / 4;
	const uint8_t *Q0 = P, *Q1 = P + qn, *Q2 = P + 2 * qn, *Q3 = P + 3 * qn;
	uint32_t h[4][32] = {};
	for (size_t i = 0; i < qn; i++) {
		h[0][Q0[i] & 31]++;
		h[1][Q1[i] & 31]++;
		h[2][Q2[i] & 31]++;
		h[3][Q3[i] & 31]++;
	}
	for (size_t i = 4 * qn; i < n2; i++) h[3][P[i] & 31]++;
	uint32_t cnt[33];
	uint32_t f[4][32];
	cnt[0] = 0;
	for (int k = 0; k < 32; k++) {
		f[0][k] = cnt[k];
		f[1][k] = f[0][k] + h[0][k];
		f[2][k] = f[1][k] + h[1][k];
		f[3][k] = f[2][k] + h[2][k];
		cnt[k + 1] = f[3][k] + h[3][k];
	}
	std::unique_ptr<uint32_t[]> occ(new uint32_t[n2 + 1]);
	for (size_t i = 0; i < qn; i++) {
		occ[f[0][Q0[i] & 31]++] = (uint32_t)i;
		occ[f[1][Q1[i] & 31]++] = (uint32_t)(i + qn);
		occ[f[2][Q2[i] & 31]++] = (uint32_t)(i + 2 * qn);
		occ[f[3][Q3[i] & 31]++] = (uint32_t)(i + 3 * qn);
	}
	for (size_t i = 4 * qn; i < n2; i++) occ[f[3][P[i] & 31]++] = (uint32_t)i;

	size_t part1 = n2, part2 = n2;

	std::unique_ptr<uint64_t[]> stk(new uint64_t[n2 + 2]);
	const uint8x16_t k20 = vdupq_n_u8(0x20);
	for (int k = 1; k <= 26; k++) {
		uint64_t *st = stk.get();
		size_t depth = 0;
		size_t size = 0;
		uint8_t top = 0;
		size_t pos = 0;
		auto segment = [&](size_t s0, size_t o) {
			if (__builtin_expect((top ^ P[s0]) == 0x20, 0)) {
				while (depth && s0 < o) {
					uint64_t e = st[depth - 1];
					uint32_t a = (uint32_t)e, b = (uint32_t)(e >> 32);
					size_t cap = std::min<size_t>(b - a, o - s0);
					size_t m = 0;
					for (;;) {
						uint8x16_t x = vld1q_u8(RP + n2 - b + m);
						uint8x16_t y = vld1q_u8(P + s0 + m);
						uint64_t mask = nibble_mask(vceqq_u8(veorq_u8(x, y), k20));
						if (mask != ~0ULL) {
							m += __builtin_ctzll(~mask) >> 2;
							break;
						}
						m += 16;
						if (m >= cap) break;
					}
					if (m > cap) m = cap;
					b -= m;
					s0 += m;
					size -= m;
					if (b == a) {
						depth--;
					} else {
						st[depth - 1] = a | ((uint64_t)b << 32);
						break;
					}
				}
				st[depth] = s0 | ((uint64_t)o << 32);
				depth += s0 < o;
				size += o - s0;
				top = depth ? P[(st[depth - 1] >> 32) - 1] : 0;
			} else {
				st[depth] = s0 | ((uint64_t)o << 32);
				depth += s0 < o;
				size += o - s0;
				top = s0 < o ? P[o - 1] : top;
			}
		};
		for (uint32_t q = cnt[k]; q < cnt[k + 1]; q++) {
			size_t o = occ[q];
			segment(pos, o);
			pos = o + 1;
		}
		segment(pos, n2);
		part2 = std::min(part2, size);
	}

	printf("Day 05 Part 1: %ld\nDay 05 Part 2: %ld\n", part1, part2);
}
