#include "advent2018.h"
#include <arm_neon.h>

namespace {

constexpr int PLANES = 5;
inline uint32_t ctz32(uint32_t x) {
	return x ? __builtin_ctz(x) : 32;
}

inline uint32_t umin(uint32_t a, uint32_t b) {
	return a < b ? a : b;
}

inline uint32_t umax(uint32_t a, uint32_t b) {
	return a > b ? a : b;
}

inline void insert_sorted(uint32_t *L, uint32_t key) {
	for (int i = 4; i > 0; i--) {
		L[i] = umin(umax(L[i - 1], key), L[i]);
	}
	L[0] = umin(L[0], key);
}

}

void day07(input_t input) {
	alignas(16) uint8_t M[32 * 32];
	uint8x16_t z = vdupq_n_u8(0);
	for (int i = 0; i < 28 * 2; i++) {
		vst1q_u8(M + 16 * i, z);
	}

	size_t nlines = (size_t(input.len) + 48) / 49;
	const uint8_t *s = (const uint8_t *) input.s;
	uint32_t chk = 0;
	size_t i = 0;
	for (; i + 4 <= nlines; i += 4, s += 4 * 49) {
		uint32_t a0 = uint8_t(s[  5] - 'A'), b0 = uint8_t(s[ 36] - 'A');
		uint32_t a1 = uint8_t(s[ 54] - 'A'), b1 = uint8_t(s[ 85] - 'A');
		uint32_t a2 = uint8_t(s[103] - 'A'), b2 = uint8_t(s[134] - 'A');
		uint32_t a3 = uint8_t(s[152] - 'A'), b3 = uint8_t(s[183] - 'A');
		chk |= (a0 + 6) | (b0 + 6) | (a1 + 6) | (b1 + 6) | (a2 + 6) | (b2 + 6) | (a3 + 6) | (b3 + 6);
		M[((a0 << 5) | b0) & 1023] = 1;
		M[((a1 << 5) | b1) & 1023] = 1;
		M[((a2 << 5) | b2) & 1023] = 1;
		M[((a3 << 5) | b3) & 1023] = 1;
	}
	for (; i < nlines; i++, s += 49) {
		uint32_t a0 = uint8_t(s[5] - 'A'), b0 = uint8_t(s[36] - 'A');
		chk |= (a0 + 6) | (b0 + 6);
		M[((a0 << 5) | b0) & 1023] = 1;
	}
	if (chk >= 32) {
		fprintf(stderr, "bad input\n");
		abort();
	}

	static const int8_t shifts[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7 };
	int8x16_t sh = vld1q_s8(shifts);
	alignas(16) uint32_t out[36];
	uint8x16_t cnt0 = z, cnt1 = z;
	for (int g = 0; g < 7; g++) {
		uint8x16_t p[4];
		for (int r = 0; r < 4; r++) {
			uint8x16_t v0 = vld1q_u8(M + 32 * (4 * g + r));
			uint8x16_t v1 = vld1q_u8(M + 32 * (4 * g + r) + 16);
			cnt0 = vaddq_u8(cnt0, v0);
			cnt1 = vaddq_u8(cnt1, v1);
			p[r] = vpaddq_u8(vshlq_u8(v0, sh), vshlq_u8(v1, sh));
		}
		uint8x16_t q = vpaddq_u8(vpaddq_u8(p[0], p[1]), vpaddq_u8(p[2], p[3]));
		vst1q_u32(out + 4 * g, vreinterpretq_u32_u8(q));
	}
	out[28] = out[29] = out[30] = out[31] = out[32] = 0;

	uint8x16_t pl[5];
	for (int k = 0; k < PLANES; k++) {
		uint8x16_t t0 = vshlq_u8(vandq_u8(vshlq_u8(cnt0, vdupq_n_s8(-k)), vdupq_n_u8(1)), sh);
		uint8x16_t t1 = vshlq_u8(vandq_u8(vshlq_u8(cnt1, vdupq_n_s8(-k)), vdupq_n_u8(1)), sh);
		pl[k] = vpaddq_u8(t0, t1);
	}
	uint8x16_t qa = vpaddq_u8(vpaddq_u8(pl[0], pl[1]), vpaddq_u8(pl[2], pl[3]));
	uint8x16_t qb = vpaddq_u8(vpaddq_u8(pl[4], pl[4]), vpaddq_u8(pl[4], pl[4]));
	uint32_t P0[PLANES];
	P0[0] = vgetq_lane_u32(vreinterpretq_u32_u8(qa), 0);
	P0[1] = vgetq_lane_u32(vreinterpretq_u32_u8(qa), 1);
	P0[2] = vgetq_lane_u32(vreinterpretq_u32_u8(qa), 2);
	P0[3] = vgetq_lane_u32(vreinterpretq_u32_u8(qa), 3);
	P0[4] = vgetq_lane_u32(vreinterpretq_u32_u8(qb), 0);

	uint64_t Q0 = P0[0] | uint64_t(P0[0]) << 32;
	uint64_t Q1 = P0[1] | uint64_t(P0[1]) << 32;
	uint64_t Q2 = P0[2] | uint64_t(P0[2]) << 32;
	uint64_t Q3 = P0[3] | uint64_t(P0[3]) << 32;
	uint64_t Q4 = P0[4] | uint64_t(P0[4]) << 32;

	char part1[27];
	uint32_t A = ~((P0[0] | P0[1]) | (P0[2] | P0[3]) | P0[4]) & 0x3ffffff;

	uint32_t L[5] = { ~0u, ~0u, ~0u, ~0u, ~0u };
	uint32_t A2 = A;
	uint32_t t = 0;
	int running = 0;
	for (; running < 5 && A2; running++) {
		uint32_t m = __builtin_ctz(A2);
		A2 &= A2 - 1;
		insert_sorted(L, ((61 + m) << 5) | m);
	}
	uint32_t L0 = L[0], L1 = L[1], L2 = L[2], L3 = L[3], L4 = L[4];

	for (int i = 0; i < 26; i++) {
		uint64_t one = Q0 & ~((Q1 | Q2) | (Q3 | Q4));
		uint32_t m = ctz32(A);
		part1[i] = 'A' + m;
		uint32_t x1 = out[m];

		uint32_t mn = L0;
		t = mn >> 5;
		uint32_t x2 = out[mn & 31];
		L0 = L1; L1 = L2; L2 = L3; L3 = L4;
		running--;
		bool gate = (L0 >> 5) != t;

		uint64_t x = x1 | uint64_t(x2) << 32;
		uint64_t nb;
		nb = ~Q0 & x; Q0 ^= x; x = nb;
		nb = ~Q1 & x; Q1 ^= x; x = nb;
		nb = ~Q2 & x; Q2 ^= x; x = nb;
		nb = ~Q3 & x; Q3 ^= x; x = nb;
		Q4 ^= x;

		A = (A & (A - 1)) | (x1 & uint32_t(one));
		A2 |= x2 & uint32_t(one >> 32);

		int lim = gate ? 5 - running : 0;
		uint32_t base = t + 61;
		uint32_t S0 = A2;
		uint32_t m0 = ctz32(S0);
		uint32_t S1 = S0 & (S0 - 1);
		uint32_t m1 = ctz32(S1);
		uint32_t S2 = S1 & (S1 - 1);
		bool v0 = (lim > 0) & (S0 != 0);
		bool v1 = (lim > 1) & (S1 != 0);
		uint32_t K0 = v0 ? ((base + m0) << 5) | m0 : ~0u;
		uint32_t K1 = v1 ? ((base + m1) << 5) | m1 : ~0u;
		A2 = v1 ? S2 : v0 ? S1 : S0;
		running += v0 + v1;

		uint32_t N0 = umin(L0, K0);
		uint32_t N1 = umin(umin(K1, L1), umax(L0, K0));
		uint32_t N2 = umin(L2, umin(umax(L0, K1), umax(L1, K0)));
		uint32_t N3 = umin(L3, umin(umax(L1, K1), umax(L2, K0)));
		uint32_t N4 = umin(umax(L2, K1), umax(L3, K0));
		L0 = N0; L1 = N1; L2 = N2; L3 = N3; L4 = N4;

		if (__builtin_expect((lim > 2) & (S2 != 0), 0)) {
			uint32_t T[5] = { L0, L1, L2, L3, L4 };
			for (; running < 5 && A2; running++) {
				uint32_t m = __builtin_ctz(A2);
				A2 &= A2 - 1;
				insert_sorted(T, ((base + m) << 5) | m);
			}
			L0 = T[0]; L1 = T[1]; L2 = T[2]; L3 = T[3]; L4 = T[4];
		}
	}
	int part2 = t;
	int n1 = 26;

	char buf[96];
	static const char h1[] = "Day 07 Part 1: ";
	static const char h2[] = "\nDay 07 Part 2: ";
	char *q = buf;
	memcpy(q, h1, sizeof h1 - 1);
	q += sizeof h1 - 1;
	memcpy(q, part1, n1);
	q += n1;
	memcpy(q, h2, sizeof h2 - 1);
	q += sizeof h2 - 1;
	char digits[12], *d = digits + sizeof digits;
	uint32_t v = part2;
	do {
		*--d = '0' + v % 10;
		v /= 10;
	} while (v);
	size_t nd = digits + sizeof digits - d;
	memcpy(q, d, nd);
	q += nd;
	*q++ = '\n';
	fwrite(buf, 1, q - buf, stdout);
}
