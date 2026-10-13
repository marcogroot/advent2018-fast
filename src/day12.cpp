#include "advent2018.h"
#include <arm_neon.h>

namespace {

inline int64_t bits_score(const uint64_t *w, int64_t lo, int64_t hi, int64_t base) {
	int64_t cnt = 0, idx = 0;
	for (int64_t i = lo; i < hi; i++) {
		uint64_t x = w[i];
		cnt += __builtin_popcountll(x);
		while (x) {
			idx += i * 64 + __builtin_ctzll(x);
			x &= x - 1;
		}
	}
	return cnt * base + idx;
}

inline int64_t bits_count(const uint64_t *w, int64_t lo, int64_t hi) {
	int64_t cnt = 0;
	for (int64_t i = lo; i < hi; i++) cnt += __builtin_popcountll(w[i]);
	return cnt;
}

}

void day12(input_t input) {
	const char *s = input.s;
	const char *end = input.s + input.len;
	while (s < end && *s != '.' && *s != '#' && *s != '\n') s++;

	const char *line = s;
	const char *nl = (const char *)memchr(s, '\n', end - s);
	s = nl ? nl : end;
	int64_t ncells = s - line;

	int64_t N = 32;
	while (N * 64 < 3 * ncells + 64 * 16) N *= 2;
	uint64_t sa[64], sb[64];
	std::vector<uint64_t> A, B;
	uint64_t *cur, *nxt;
	if (N <= 64) {
		N = 64;
		memset(sa, 0, sizeof(sa));
		memset(sb, 0, sizeof(sb));
		cur = sa; nxt = sb;
	} else {
		A.assign(N, 0); B.assign(N, 0);
		cur = A.data(); nxt = B.data();
	}

	int64_t lo = N / 4, hi;
	int64_t base = -lo * 64;
	{
		static const uint8_t wts[16] = {1,2,4,8,16,32,64,128,1,2,4,8,16,32,64,128};
		uint8x16_t wv = vld1q_u8(wts);
		uint8x16_t one = vdupq_n_u8(1);
		char *dst = (char *)(cur + lo);
		int64_t k = 0;
		for (; k + 16 <= ncells; k += 16) {
			uint8x16_t v = vld1q_u8((const uint8_t *)line + k);
			uint8x16_t m = vandq_u8(vtstq_u8(v, one), wv);
			uint16_t bits = (uint16_t)(vaddv_u8(vget_low_u8(m)) | (vaddv_u8(vget_high_u8(m)) << 8));
			memcpy(dst, &bits, 2);
			dst += 2;
		}
		for (; k < ncells; k++) {
			if (line[k] == '#') {
				int64_t bp = lo * 64 + k;
				cur[bp >> 6] |= 1ULL << (bp & 63);
			}
		}
	}
	hi = lo + (ncells + 63) / 64;
	while (lo < hi && !cur[lo]) lo++;
	while (hi > lo && !cur[hi - 1]) hi--;

	uint32_t rules = 0;
	while (s < end) {
		if (*s == '\n') { s++; continue; }
		if ((s[0] == '#' || s[0] == '.') && s[5] == ' ' && s + 10 < end && s[10] == '\n') {
			uint32_t n = ((s[0] & 1) << 4) | ((s[1] & 1) << 3) | ((s[2] & 1) << 2) | ((s[3] & 1) << 1) | (s[4] & 1);
			rules |= (uint32_t)(s[9] & 1) << n;
			s += 10;
			continue;
		}
		uint32_t n = 0;
		for (; s < end && *s != '\n'; s++) {
			if (*s == '.') n <<= 1;
			else if (*s == '#') n = (n << 1) | 1;
		}
		if (s < end) rules |= (n & 1) << ((n >> 1) & 31);
	}

	uint64x2_t mv[32];
	for (int i = 0; i < 32; i++) mv[i] = vdupq_n_u64(-(uint64_t)((rules >> i) & 1));

	int64_t part1 = 0, part2 = 0;
	int64_t plo = lo, phi = lo;
	for (int64_t gen = 0; ; ) {
		if (lo >= hi) { part1 = part2 = 0; break; }
		if (lo < 3 || hi + 4 > N) {
			int64_t width = hi - lo;
			if (width + 16 > N) {
				int64_t NN = N;
				while (width + 16 > NN) NN *= 2;
				std::vector<uint64_t> C(NN, 0), D(NN, 0);
				int64_t nlo = (NN - width) / 2;
				for (int64_t i = 0; i < width; i++) C[nlo + i] = cur[lo + i];
				base += (lo - nlo) * 64;
				A.swap(C); B.swap(D);
				N = NN;
				cur = A.data(); nxt = B.data();
				lo = nlo; hi = nlo + width;
			} else {
				int64_t nlo = (N - width) / 2;
				std::vector<uint64_t> tmp(cur + lo, cur + hi);
				for (int64_t i = 0; i < N; i++) cur[i] = 0;
				for (int64_t i = 0; i < N; i++) nxt[i] = 0;
				for (int64_t i = 0; i < width; i++) cur[nlo + i] = tmp[i];
				base += (lo - nlo) * 64;
				lo = nlo; hi = nlo + width;
			}
			plo = phi = lo;
		}

		int64_t olo = lo - ((cur[lo] & 3) != 0), ohi = hi + ((cur[hi - 1] >> 62) != 0);
		uint64x2_t dm2 = vdupq_n_u64(0), dm1 = dm2, d0 = dm2, dp1 = dm2, dp2 = dm2;
		for (int64_t i = olo; i < ohi; i += 2) {
			uint64x2_t c = vld1q_u64(cur + i);
			uint64x2_t p = vld1q_u64(cur + i - 1);
			uint64x2_t n = vld1q_u64(cur + i + 1);
			uint64x2_t xm2 = vsliq_n_u64(vshrq_n_u64(p, 62), c, 2);
			uint64x2_t xm1 = vsliq_n_u64(vshrq_n_u64(p, 63), c, 1);
			uint64x2_t xp1 = vsriq_n_u64(vshlq_n_u64(n, 63), c, 1);
			uint64x2_t xp2 = vsriq_n_u64(vshlq_n_u64(n, 62), c, 2);
			uint64x2_t v1[16];
			for (int j = 0; j < 16; j++) v1[j] = vbslq_u64(xp2, mv[2*j+1], mv[2*j]);
			uint64x2_t v2[8];
			for (int j = 0; j < 8; j++) v2[j] = vbslq_u64(xp1, v1[2*j+1], v1[2*j]);
			uint64x2_t v3[4];
			for (int j = 0; j < 4; j++) v3[j] = vbslq_u64(c, v2[2*j+1], v2[2*j]);
			uint64x2_t v4a = vbslq_u64(xm1, v3[1], v3[0]);
			uint64x2_t v4b = vbslq_u64(xm1, v3[3], v3[2]);
			uint64x2_t r = vbslq_u64(xm2, v4b, v4a);
			vst1q_u64(nxt + i, r);
			dm2 = vorrq_u64(dm2, veorq_u64(r, xm2));
			dm1 = vorrq_u64(dm1, veorq_u64(r, xm1));
			d0 = vorrq_u64(d0, veorq_u64(r, c));
			dp1 = vorrq_u64(dp1, veorq_u64(r, xp1));
			dp2 = vorrq_u64(dp2, veorq_u64(r, xp2));
		}
		int64_t wend = olo + ((ohi - olo + 1) & ~int64_t(1));
		for (int64_t i = plo; i < olo; i++) nxt[i] = 0;
		for (int64_t i = wend; i < phi; i++) nxt[i] = 0;

		int64_t nlo = olo, nhi = ohi;
		while (nlo < nhi && !nxt[nlo]) nlo++;
		while (nhi > nlo && !nxt[nhi - 1]) nhi--;

		if (++gen >= 20) {
			if (gen == 20) {
				part1 = bits_score(nxt, nlo, nhi, base);
			} else {
				uint64_t z[5] = {
					vgetq_lane_u64(dm2, 0) | vgetq_lane_u64(dm2, 1),
					vgetq_lane_u64(dm1, 0) | vgetq_lane_u64(dm1, 1),
					vgetq_lane_u64(d0, 0) | vgetq_lane_u64(d0, 1),
					vgetq_lane_u64(dp1, 0) | vgetq_lane_u64(dp1, 1),
					vgetq_lane_u64(dp2, 0) | vgetq_lane_u64(dp2, 1),
				};
				int d = 3;
				for (int k = 0; k < 5; k++) if (!z[k]) { d = 2 - k; break; }
				if (d != 3) {
					int64_t cnt = bits_count(nxt, nlo, nhi);
					part2 = bits_score(nxt, nlo, nhi, base);
					part2 += cnt * d * (50000000000LL - gen);
					break;
				}
			}
		}

		plo = lo; phi = hi;
		uint64_t *t = cur; cur = nxt; nxt = t;
		lo = nlo; hi = nhi;
	}

	printf("Day 12 Part 1: %ld\nDay 12 Part 2: %ld\n", part1, part2);
}
