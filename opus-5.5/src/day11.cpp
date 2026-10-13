#include "advent2018.h"
#include <arm_neon.h>

namespace {

constexpr int DIM = 300;
constexpr int ST = 320;

struct answer_t {
	int32_t x, y, sz;
};

struct grid_t {
	alignas(64) int16_t s[DIM + 1][ST];
};

inline int16x8_t window(const int16_t *r0, const int16_t *r1, int c, int s) {
	int16x8_t a = vld1q_s16(r0 + c);
	int16x8_t b = vld1q_s16(r0 + c + s);
	int16x8_t e = vld1q_s16(r1 + c);
	int16x8_t d = vld1q_s16(r1 + c + s);
	return vsubq_s16(vaddq_s16(a, d), vaddq_s16(b, e));
}

template <int K>
void size_max_group(const grid_t &G, int s, int16_t *out) {
	const int n0 = DIM + 1 - s;
	const int nl = n0 - (K - 1);
	const int full = nl & ~7;
	static const int16_t iota_arr[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	const int16x8_t iota = vld1q_s16(iota_arr);
	const int16x8_t vmin = vdupq_n_s16(INT16_MIN);
	int16x8_t m[K], t[K];
	for (int i = 0; i < K; i++) {
		m[i] = vmin;
		t[i] = vmin;
	}
	for (int y = 1; y <= nl; y++) {
		const int16_t *r0 = G.s[y - 1];
		const int16_t *r1 = G.s[y + s - 1];
		int c = 0;
		for (; c < full; c += 8) {
			int16x8_t a = vld1q_s16(r0 + c);
#pragma unroll
			for (int i = 0; i < K; i++) {
				int16x8_t b = vld1q_s16(r0 + c + s + i);
				int16x8_t e = vld1q_s16(r1 + i * ST + c);
				int16x8_t d = vld1q_s16(r1 + i * ST + c + s + i);
				m[i] = vmaxq_s16(m[i], vsubq_s16(vaddq_s16(a, d), vaddq_s16(b, e)));
			}
		}
		for (; c < n0; c += 8) {
			int16x8_t a = vld1q_s16(r0 + c);
			int16x8_t lane = vaddq_s16(iota, vdupq_n_s16(c));
#pragma unroll
			for (int i = 0; i < K; i++) {
				int16x8_t b = vld1q_s16(r0 + c + s + i);
				int16x8_t e = vld1q_s16(r1 + i * ST + c);
				int16x8_t d = vld1q_s16(r1 + i * ST + c + s + i);
				uint16x8_t ok = vcltq_s16(lane, vdupq_n_s16(n0 - i));
				t[i] = vmaxq_s16(t[i], vbslq_s16(ok, vsubq_s16(vaddq_s16(a, d), vaddq_s16(b, e)), vmin));
			}
		}
	}
	for (int y = nl + 1; y <= n0; y++) {
		const int16_t *r0 = G.s[y - 1];
		for (int i = 0; i < K; i++) {
			const int ni = n0 - i;
			if (y > ni) {
				break;
			}
			const int si = s + i;
			const int16_t *r1 = G.s[y + si - 1];
			for (int c = 0; c < ni; c += 8) {
				int16x8_t lane = vaddq_s16(iota, vdupq_n_s16(c));
				uint16x8_t ok = vcltq_s16(lane, vdupq_n_s16(ni));
				t[i] = vmaxq_s16(t[i], vbslq_s16(ok, window(r0, r1, c, si), vmin));
			}
		}
	}
	for (int i = 0; i < K; i++) {
		out[i] = vmaxvq_s16(vmaxq_s16(m[i], t[i]));
	}
}

answer_t locate(const grid_t &G, int s, int16_t M) {
	const int n = DIM + 1 - s;
	int16x8_t vm = vdupq_n_s16(M);
	for (int y = 1; y <= n; y++) {
		const int16_t *r0 = G.s[y - 1];
		const int16_t *r1 = G.s[y + s - 1];
		for (int c = 0; c < n; c += 8) {
			uint16x8_t eq = vceqq_s16(window(r0, r1, c, s), vm);
			if (vmaxvq_u16(eq)) {
				uint16_t lanes[8];
				vst1q_u16(lanes, eq);
				for (int i = 0; i < 8 && c + i < n; i++) {
					if (lanes[i]) {
						return { c + i + 1, y, s };
					}
				}
			}
		}
	}
	return { 0, 0, 0 };
}

int sigma(double S) {
	return ceil(sqrt(33) * S);
}

}

void day11(input_t input) {
	int32_t in = 0;
	for (; input.len--; input.s++) {
		uint8_t c = *input.s - '0';
		if (c < 10) {
			in = 10 * in + c;
		}
	}

	static grid_t G;
	memset(G.s[0], 0, sizeof(G.s[0]));

	alignas(64) uint16_t r[ST] = { };
	alignas(64) uint16_t step[ST] = { };
	for (int x = 1; x <= DIM; x++) {
		uint32_t rack = x + 10;
		step[x] = (rack * rack) % 1000;
		r[x] = ((uint32_t) in * rack) % 1000;
	}

	const int16x8_t zero = vdupq_n_s16(0);
	const uint16x8_t k1000 = vdupq_n_u16(1000);
	const uint16x8_t k41 = vdupq_n_u16(41);
	const int16x8_t k5 = vdupq_n_s16(5);
	for (int y = 1; y <= DIM; y++) {
		const int16_t *prev = G.s[y - 1];
		int16_t *cur = G.s[y];
		int16x8_t carry = zero;
		for (int c = 0; c < ST; c += 8) {
			uint16x8_t rv = vaddq_u16(vld1q_u16(r + c), vld1q_u16(step + c));
			rv = vminq_u16(rv, vsubq_u16(rv, k1000));
			vst1q_u16(r + c, rv);
			int16x8_t v = vsubq_s16(vreinterpretq_s16_u16(vshrq_n_u16(vmulq_u16(rv, k41), 12)), k5);
			v = vaddq_s16(v, vextq_s16(zero, v, 7));
			v = vaddq_s16(v, vextq_s16(zero, v, 6));
			v = vaddq_s16(v, vextq_s16(zero, v, 4));
			v = vaddq_s16(v, carry);
			carry = vdupq_laneq_s16(v, 7);
			vst1q_s16(cur + c, vaddq_s16(v, vld1q_s16(prev + c)));
		}
	}

	int16_t max3;
	size_max_group<1>(G, 3, &max3);
	answer_t P1 = locate(G, 3, max3);

	int smax = sigma(6.0);
	int16_t maxes[64];
	int s = 4;
	for (; s + 4 <= smax + 1; s += 4) {
		size_max_group<4>(G, s, maxes + s);
	}
	for (; s <= smax; s++) {
		size_max_group<1>(G, s, maxes + s);
	}
	int32_t best = INT32_MIN;
	int best_s = 4;
	for (s = 4; s <= smax; s++) {
		if (maxes[s] > best) {
			best = maxes[s];
			best_s = s;
		}
	}
	answer_t P2 = locate(G, best_s, (int16_t) best);

	printf("Day 11 Part 1: %d,%d\nDay 11 Part 2: %d,%d,%d\n",
			P1.x, P1.y,
			P2.x, P2.y, P2.sz);
}
