#include "advent2018.h"
#include <arm_neon.h>

namespace {

enum : uint8_t { OP_OTHER, OP_SETI, OP_BORI, OP_BANI, OP_MULI, OP_GTIR };

struct Insn {
	uint8_t op;
	uint32_t a, b, c;
};

struct Params {
	uint32_t seed = 0, ork = 65536, mask = 0xffffff, mul = 65899, bytemask = 255, div = 256;
	bool ok = false;
};

static inline uint32_t parse_uint(const char *&s) {
	uint32_t v = 0;
	for (uint8_t c; (c = *s - '0') < 10; s++) {
		v = 10 * v + c;
	}
	return v;
}

static Params parse(input_t input) {
	const char *s = input.s, *end = input.s + input.len;
	while (s < end && *s != '\n') s++;
	Insn prog[64];
	int n = 0;
	while (s < end && n < 64) {
		while (s < end && (*s == '\n' || *s == '\r' || *s == ' ')) s++;
		if (s + 4 >= end) break;
		uint32_t w;
		memcpy(&w, s, 4);
		uint8_t op = OP_OTHER;
		if (w == 0x69746573) op = OP_SETI;
		else if (w == 0x69726f62) op = OP_BORI;
		else if (w == 0x696e6162) op = OP_BANI;
		else if (w == 0x696c756d) op = OP_MULI;
		else if (w == 0x72697467) op = OP_GTIR;
		s += 5;
		Insn &I = prog[n++];
		I.op = op;
		I.a = parse_uint(s); s++;
		I.b = parse_uint(s); s++;
		I.c = parse_uint(s);
		while (s < end && *s != '\n') s++;
	}

	Params P;
	int i = 0;
	while (i < n && prog[i].op != OP_BORI) i++;
	if (i >= n) return P;
	P.ork = prog[i].b;
	uint32_t areg = prog[i].c;
	if (++i >= n || prog[i].op != OP_SETI) return P;
	P.seed = prog[i].a;
	uint32_t hreg = prog[i].c;
	int got = 0;
	for (i++; i < n; i++) {
		const Insn &I = prog[i];
		if (I.op == OP_BANI && I.a == areg && !(got & 1)) { P.bytemask = I.b; got |= 1; }
		else if (I.op == OP_BANI && I.a == hreg && I.c == hreg && !(got & 2)) { P.mask = I.b; got |= 2; }
		else if (I.op == OP_MULI && I.a == hreg && I.c == hreg && !(got & 4)) { P.mul = I.b; got |= 4; }
		else if (I.op == OP_GTIR && I.b == areg && !(got & 8)) { P.div = I.a; got |= 8; }
	}
	P.ok = got == 15 && P.div > 1;
	return P;
}

template <typename F>
static void solve(F f, uint32_t mask, uint32_t &part1, uint32_t &part2, uint32_t *V, size_t cap, std::vector<uint32_t> &big) {
	uint32_t *buf = V;
	uint32_t cp[16];
	size_t cpi[16];
	for (int k = 0; k < 16; k++) cp[k] = ~0u;
	uint32x4_t c0 = vdupq_n_u32(~0u), c1 = c0, c2 = c0, c3 = c0;
	size_t ncp = 0, s = 1;
	size_t n = 0;
	uint32_t y = 0, x = 0;
	buf[n++] = x;
	cpi[ncp] = 0;
	cp[ncp++] = x;
	c0 = vsetq_lane_u32(x, c0, 0);
	auto grow = [&]() {
		big.assign(buf, buf + n);
		big.resize(cap * 2 + 16);
		cap *= 2;
		buf = big.data();
	};
	auto step = [&]() -> bool {
		y = f(y);
		x = y & mask;
		buf[n] = x;
		uint32x4_t xv = vdupq_n_u32(x);
		uint32x4_t m = vorrq_u32(vorrq_u32(vceqq_u32(xv, c0), vceqq_u32(xv, c1)),
				vorrq_u32(vceqq_u32(xv, c2), vceqq_u32(xv, c3)));
		return vmaxvq_u32(m) != 0;
	};
	auto checkpoint = [&]() {
		if (ncp == 16) {
			for (int k = 0; k < 8; k++) cp[k] = cp[2 * k], cpi[k] = cpi[2 * k];
			for (int k = 8; k < 16; k++) cp[k] = ~0u;
			ncp = 8;
			s <<= 1;
		}
		if ((n & (s - 1)) == 0) {
			cpi[ncp] = n;
			cp[ncp++] = x;
			c0 = vld1q_u32(cp);
			c1 = vld1q_u32(cp + 4);
			c2 = vld1q_u32(cp + 8);
			c3 = vld1q_u32(cp + 12);
		}
	};
	for (;;) {
		if (step()) goto found;
		if ((n & (s - 1)) == 0) checkpoint();
		n++;
		if (n >= 65) break;
	}
	for (;;) {
		if (__builtin_expect(n + 2 > cap, 0)) grow();
		if (__builtin_expect(step(), 0)) goto found;
		n++;
		if (__builtin_expect(step(), 0)) goto found;
		if (__builtin_expect((n & (s - 1)) == 0, 0)) checkpoint();
		n++;
	}
found:
	part1 = buf[1];
	size_t k = 0;
	while (cp[k] != x) k++;
	size_t t = cpi[k];
	size_t p = n - t;
	for (size_t q = 0; q < 8; q++) buf[n + 1 + q] = ~0u;
	size_t lo = t >= s ? t - s : 0;
	const uint32_t *tp = buf + lo, *hp = buf + lo + p;
	for (;;) {
		uint32x4_t e0 = vceqq_u32(vld1q_u32(hp), vld1q_u32(tp));
		uint32x4_t e1 = vceqq_u32(vld1q_u32(hp + 4), vld1q_u32(tp + 4));
		uint16x8_t e = vcombine_u16(vmovn_u32(e0), vmovn_u32(e1));
		uint64_t mm = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(e, 4)), 0);
		if (mm) {
			int off = __builtin_ctzll(mm) >> 3;
			part2 = hp[off - 1];
			break;
		}
		hp += 8;
		tp += 8;
	}
}

}

void day21(input_t input) {
	Params P = parse(input);
	if (!P.ok) {
		fprintf(stderr, "Day 21 unrecognized program\n");
		abort();
	}

	uint32_t part1 = 0, part2 = 0;
	constexpr size_t CAP = 1 << 15;
	alignas(64) uint32_t store[CAP + 16];
	std::vector<uint32_t> big;

	if (P.ork == 65536 && P.mask == 0xffffff && P.bytemask == 255 && P.div == 256) {
		const uint32_t M1 = P.mul, M2 = M1 * M1, M3 = M2 * M1;
		const uint32_t C = P.seed * M3, CM = C + M1, M1x2 = 2 * M1;
		const uint32_t mask = P.mask;
		auto f = [=](uint32_t h) -> uint32_t {
			uint32_t b0, b1, b2, p0, q, r;
			__asm__(
				"and %w[b0], %w[h], #255\n\t"
				"ubfx %w[b1], %w[h], #8, #8\n\t"
				"ubfx %w[b2], %w[h], #17, #7\n\t"
				"mul %w[p0], %w[b0], %w[m3]\n\t"
				"madd %w[q], %w[b2], %w[m1x2], %w[cm]\n\t"
				"madd %w[p0], %w[b1], %w[m2], %w[p0]\n\t"
				"add %w[r], %w[p0], %w[q]"
				: [b0] "=&r"(b0), [b1] "=&r"(b1), [b2] "=&r"(b2), [p0] "=&r"(p0), [q] "=&r"(q), [r] "=r"(r)
				: [h] "r"(h), [m1x2] "r"(M1x2), [m2] "r"(M2), [m3] "r"(M3), [cm] "r"(CM));
			return r;
		};
		solve(f, mask, part1, part2, store, CAP, big);
	} else {
		const Params Q = P;
		auto f = [=](uint32_t h) -> uint32_t {
			uint64_t a = h | Q.ork;
			uint64_t x = Q.seed;
			for (;;) {
				x = (((x + (a & Q.bytemask)) & Q.mask) * Q.mul) & Q.mask;
				if (a < Q.div) break;
				a /= Q.div;
			}
			return (uint32_t) x;
		};
		solve(f, ~0u, part1, part2, store, CAP, big);
	}

	printf("Day 21 Part 1: %u\nDay 21 Part 2: %u\n", part1, part2);
}
