#include "advent2018.h"
#include <arm_neon.h>
#include <memory>

namespace {

constexpr int STRIDE = 17;
constexpr int ROWS = 1088;
constexpr int BANDSHIFT = 4;
constexpr int NBANDS = (ROWS >> BANDSHIFT) + 1;
constexpr int PLANE = STRIDE * ROWS + 2;

struct claim_t {
	uint64x2_t mask;
	uint32_t off, h, line, pad;
};

inline uint64_t load8(const char *p) {
	uint64_t w;
	memcpy(&w, p, 8);
	return w;
}

inline uint64_t nondigit(uint64_t w) {
	uint64_t a = (w ^ 0x3030303030303030ull) & 0xF0F0F0F0F0F0F0F0ull;
	uint64_t b = ((w & 0x0F0F0F0F0F0F0F0Full) + 0x0606060606060606ull) & 0x1010101010101010ull;
	return a | b;
}

inline uint32_t parse_num(const char *&p) {
	uint64_t w = load8(p);
	unsigned len = __builtin_ctzll(nondigit(w)) >> 3;
	uint64_t v = (w << (64 - 8 * len)) & 0x0F0F0F0F0F0F0F0Full;
	v = (v * 10 + (v >> 8)) & 0x00FF00FF00FF00FFull;
	v = (v * 100 + (v >> 16)) & 0x0000FFFF0000FFFFull;
	v = (v * 10000 + (v >> 32)) & 0xFFFFFFFFull;
	p += len + 1;
	return v;
}

inline uint32_t parse_len(const char *p, unsigned len) {
	uint64_t w = load8(p);
	uint64_t v = (w << (64 - 8 * len)) & 0x0F0F0F0F0F0F0F0Full;
	v = (v * 10 + (v >> 8)) & 0x00FF00FF00FF00FFull;
	v = (v * 100 + (v >> 16)) & 0x0000FFFF0000FFFFull;
	v = (v * 10000 + (v >> 32)) & 0xFFFFFFFFull;
	return v;
}

inline uint64_t mask32(uint8x16_t a, uint8x16_t b) {
	const uint8x16_t bits = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
	uint8x16_t x = vpaddq_u8(vandq_u8(a, bits), vandq_u8(b, bits));
	x = vpaddq_u8(x, x);
	x = vpaddq_u8(x, x);
	return vgetq_lane_u32(vreinterpretq_u32_u8(x), 0);
}

inline uint64_t movemask64(const uint8_t *p, uint8x16_t needle) {
	const uint8x16_t bits = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
	uint8x16_t a = vandq_u8(vceqq_u8(vld1q_u8(p + 0), needle), bits);
	uint8x16_t b = vandq_u8(vceqq_u8(vld1q_u8(p + 16), needle), bits);
	uint8x16_t c = vandq_u8(vceqq_u8(vld1q_u8(p + 32), needle), bits);
	uint8x16_t d = vandq_u8(vceqq_u8(vld1q_u8(p + 48), needle), bits);
	uint8x16_t ab = vpaddq_u8(a, b);
	uint8x16_t cd = vpaddq_u8(c, d);
	uint8x16_t abcd = vpaddq_u8(ab, cd);
	abcd = vpaddq_u8(abcd, abcd);
	return vgetq_lane_u64(vreinterpretq_u64_u8(abcd), 0);
}

}

void day03(input_t input) {
	const char *s = input.s;
	const char *end = input.s + input.len;
	size_t len = input.len;

	size_t maxlines = len / 8 + 16;
	std::unique_ptr<uint32_t[]> pos(new uint32_t[maxlines + 8]);
	size_t npos = 0;
	const uint8x16_t hash = vdupq_n_u8('#');
	for (size_t i = 0; i < len; i += 64) {
		uint64_t m = movemask64((const uint8_t *)s + i, hash);
		if (i + 64 > len) m &= (len - i >= 64) ? ~0ull : ((1ull << (len - i)) - 1);
		size_t n = npos;
		npos += __builtin_popcountll(m);
		do {
			pos[n++] = i + __builtin_ctzll(m);
			m &= m - 1;
		} while (m);
	}

	std::unique_ptr<uint64_t[]> planes(new uint64_t[2 * PLANE]);
	uint64_t *seen = planes.get(), *col = planes.get() + PLANE;
	memset(planes.get(), 0, 2 * PLANE * sizeof(uint64_t));

	std::unique_ptr<claim_t[]> claims(new claim_t[2 * npos + 2]);
	claim_t *sorted = claims.get() + npos + 1;
	uint32_t bandcnt[NBANDS + 1] = {};
	size_t nclaims = 0;

	for (size_t li = 0; li < npos; li++) {
		const char *q = s + pos[li];
		uint32_t line = pos[li] + 1;
		uint8x16_t a = vld1q_u8((const uint8_t *)q), b = vld1q_u8((const uint8_t *)q + 16);
		const uint8x16_t z = vdupq_n_u8('0'), nine = vdupq_n_u8(9);
		uint64_t nd = mask32(vcgtq_u8(vsubq_u8(a, z), nine), vcgtq_u8(vsubq_u8(b, z), nine));
		uint64_t d = ~nd;
		uint64_t st = d & ~(d << 1);
		uint64_t en = nd & (d << 1);
		st &= st - 1;
		en &= en - 1;
		unsigned xs = __builtin_ctzll(st); st &= st - 1;
		unsigned xe = __builtin_ctzll(en); en &= en - 1;
		unsigned ys = __builtin_ctzll(st); st &= st - 1;
		unsigned ye = __builtin_ctzll(en); en &= en - 1;
		unsigned ws = __builtin_ctzll(st); st &= st - 1;
		unsigned we = __builtin_ctzll(en); en &= en - 1;
		unsigned hs = __builtin_ctzll(st);
		unsigned he = __builtin_ctzll(en);
		if (__builtin_expect(q + he >= end || he >= 32, 0)) break;
		uint32_t x = parse_len(q + xs, xe - xs);
		uint32_t y = parse_len(q + ys, ye - ys);
		uint32_t w = parse_len(q + ws, we - ws);
		uint32_t h = parse_len(q + hs, he - hs);

		unsigned shift = x % 64;
		uint64_t mask = (uint64_t(1) << w) - 1;
		uint64_t m0 = mask << shift;
		uint64_t m1 = shift ? (mask >> (64 - shift)) : 0;
		claim_t &c = claims[nclaims++];
		c.mask = vcombine_u64(vcreate_u64(m0), vcreate_u64(m1));
		c.off = y * STRIDE + x / 64;
		c.h = h;
		c.line = line;
		bandcnt[(y >> BANDSHIFT) + 1]++;
	}
	for (int i = 1; i <= NBANDS; i++) bandcnt[i] += bandcnt[i - 1];
	for (size_t i = 0; i < nclaims; i++) {
		const claim_t &c = claims[i];
		sorted[bandcnt[c.off / (STRIDE << BANDSHIFT)]++] = c;
	}

	claim_t *cands = claims.get();
	size_t ncand = 0;
	for (size_t i = 0; i < nclaims; i++) {
		const claim_t c = sorted[i];
		uint64x2_t mv = c.mask;
		uint64x2_t acc = vdupq_n_u64(0);
		uint64_t *sp = seen + c.off, *cp = col + c.off;
		for (uint32_t k = 0; k < c.h; k++) {
			uint64x2_t sv = vld1q_u64(sp);
			uint64x2_t cv = vld1q_u64(cp);
			uint64x2_t t = vandq_u64(sv, mv);
			acc = vorrq_u64(acc, t);
			vst1q_u64(sp, vorrq_u64(sv, mv));
			vst1q_u64(cp, vorrq_u64(cv, t));
			sp += STRIDE;
			cp += STRIDE;
		}
		cands[ncand] = c;
		ncand += vmaxvq_u32(vreinterpretq_u32_u64(acc)) == 0;
	}

	uint32x4_t total = vdupq_n_u32(0);
	const uint8_t *cb = (const uint8_t *)col;
	size_t nbytes = (size_t)STRIDE * ROWS * 8;
	for (size_t i = 0; i < nbytes; i += 256) {
		uint8x16_t s8 = vdupq_n_u8(0);
		for (int j = 0; j < 256; j += 16) {
			s8 = vaddq_u8(s8, vcntq_u8(vld1q_u8(cb + i + j)));
		}
		total = vpadalq_u16(total, vpaddlq_u8(s8));
	}
	int part1 = vaddvq_u32(total);

	int part2 = 0;
	uint32_t bestend = 0;
	for (size_t i = 0; i < ncand; i++) {
		const claim_t &c = cands[i];
		uint64x2_t acc = vdupq_n_u64(0);
		const uint64_t *cp = col + c.off;
		for (uint32_t k = 0; k < c.h; k++) {
			acc = vorrq_u64(acc, vandq_u64(vld1q_u64(cp), c.mask));
			cp += STRIDE;
		}
		uint32_t endrow = c.off / STRIDE + c.h;
		if (vmaxvq_u32(vreinterpretq_u32_u64(acc)) == 0 && endrow >= bestend) {
			bestend = endrow;
			const char *q = s + c.line;
			part2 = parse_num(q);
		}
	}

	printf("Day 03 Part 1: %d\nDay 03 Part 2: %d\n", part1, part2);
}
