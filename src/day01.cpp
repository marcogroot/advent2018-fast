#include "advent2018.h"
#include <arm_neon.h>

namespace {

static inline uint64_t load64(const char *p) {
	uint64_t x;
	memcpy(&x, p, 8);
	return x;
}

static inline uint64_t nl_mask16(const char *p) {
	uint8x16_t v = vld1q_u8((const uint8_t *)p);
	uint8x16_t eq = vceqq_u8(v, vdupq_n_u8('\n'));
	uint8x8_t n = vshrn_n_u16(vreinterpretq_u16_u8(eq), 4);
	return vget_lane_u64(vreinterpret_u64_u8(n), 0) & 0x8888888888888888ull;
}

static inline int parse_digits(const char *p, int len) {
	uint64_t x = load64(p);
	uint64_t d = (x & 0x0F0F0F0F0F0F0F0Full) << ((64 - 8 * len) & 63);
	d = (d * 10 + (d >> 8)) & 0x00FF00FF00FF00FFull;
	d = (d * 100 + (d >> 16)) & 0x0000FFFF0000FFFFull;
	d = (d * 10000 + (d >> 32)) & 0xFFFFFFFFull;
	return (int)d;
}

}

void day01(input_t input) {
	const char *base = input.s;
	size_t len = input.len;

	size_t cap = len / 2 + 16;
	size_t hcap = 16;
	while (hcap < 2 * cap) hcap <<= 1;
	size_t bytes = cap * 4 * sizeof(int) + hcap * sizeof(int);
	alignas(16) char stackbuf[131072];
	std::unique_ptr<char[]> heap;
	char *mem = stackbuf;
	if (bytes > sizeof(stackbuf)) {
		heap.reset(new char[bytes]);
		mem = heap.get();
	}
	int *s = (int *)mem, *R = s + cap, *Q = R + cap, *next = Q + cap, *H = next + cap;

	int *pos = R;
	int np = 0;
	size_t full = len & ~(size_t)15;
	for (size_t off = 0; off < len; off += 16) {
		uint64_t m = nl_mask16(base + off);
		if (off >= full) m &= (1ull << (4 * (len - off))) - 1;
		int c = __builtin_popcountll(m);
		int *w = pos + np;
		do {
			for (int k = 0; k < 6; k++) {
				w[k] = (int)off + (__builtin_ctzll(m) >> 2);
				m &= m - 1;
			}
			w += 6;
		} while (m);
		np += c;
	}

	int cnt = 0, total = 0;
	int start = 0;
	for (int k = 0; k < np; k++) {
		int e = pos[k];
		const char *b = base + start;
		int neg = -(*b == '-');
		int n = parse_digits(b + 1, e - start - 1);
		start = e + 1;
		total += (n ^ neg) - neg;
		s[cnt] = total;
		cnt += (n != 0);
	}

	int part1 = total;

	double dp = (double)part1;
	for (int i = 0; i < cnt; i++) {
		int a = s[i];
		int q = (int)((double)a / dp);
		int r = a - q * part1;
		q -= (r < 0);
		r += (r < 0) ? part1 : 0;
		R[i] = r; Q[i] = q;
	}

	int hbits = 4;
	while ((1 << hbits) < 2 * cnt) hbits++;
	memset(H, 0xFF, (sizeof(int)) << hbits);

	uint64_t best = UINT64_MAX;
	int answer = INT_MAX;
	for (int j = 0; j < cnt; j++) {
		int rj = R[j], qj = Q[j];
		uint32_t h = ((uint32_t)rj * 0x9E3779B1u) >> (32 - hbits);
		int i = H[h];
		next[j] = i;
		H[h] = j;
		for (; i >= 0; i = next[i]) {
			if (R[i] != rj) continue;
			int qi = Q[i];
			uint64_t cand;
			int val;
			if (qj >= qi) {
				cand = ((uint64_t)(uint32_t)(qj - qi) << 32) | (uint32_t)i;
				val = s[j];
			} else {
				cand = ((uint64_t)(uint32_t)(qi - qj) << 32) | (uint32_t)j;
				val = s[i];
			}
			if (cand < best) { best = cand; answer = val; }
		}
	}

	printf("Day 01 Part 1: %d\nDay 01 Part 2: %d\n", part1, answer);
}
