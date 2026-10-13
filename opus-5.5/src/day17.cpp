#include "advent2018.h"
#include <arm_neon.h>
#include <memory>

namespace {

struct Vein {
	uint16_t a, b0, b1;
	bool xfirst;
};

static inline void set_range(uint64_t *row, int a, int b) {
	int wa = a >> 6, wb = b >> 6;
	uint64_t ma = ~0ull << (a & 63);
	uint64_t mb = ~0ull >> (63 - (b & 63));
	if (wa == wb) {
		row[wa] |= ma & mb;
		return;
	}
	row[wa] |= ma;
	for (int w = wa + 1; w < wb; w++) row[w] = ~0ull;
	row[wb] |= mb;
}

static inline void set_range2(uint64_t *W, uint64_t *F, int a, int b, int &cnt) {
	int wa = a >> 6, wb = b >> 6;
	uint64_t ma = ~0ull << (a & 63);
	uint64_t mb = ~0ull >> (63 - (b & 63));
	if (wa == wb) {
		uint64_t m = ma & mb;
		cnt += __builtin_popcountll(m & ~F[wa]);
		W[wa] |= m;
		F[wa] |= m;
		return;
	}
	cnt += __builtin_popcountll(ma & ~F[wa]) + __builtin_popcountll(mb & ~F[wb]);
	W[wa] |= ma;
	F[wa] |= ma;
	W[wb] |= mb;
	F[wb] |= mb;
	for (int w = wa + 1; w < wb; w++) {
		cnt += __builtin_popcountll(~F[w]);
		W[w] = F[w] = ~0ull;
	}
}

static inline bool test(const uint64_t *row, int x) {
	return (row[x >> 6] >> (x & 63)) & 1;
}

static inline void setb(uint64_t *row, int x) {
	row[x >> 6] |= 1ull << (x & 63);
}

static inline unsigned parse_num(const char *s) {
	uint64_t x;
	memcpy(&x, s, 8);
	uint64_t nd = (x & 0xF0F0F0F0F0F0F0F0ull) ^ 0x3030303030303030ull;
	int len = __builtin_ctzll(nd) >> 3;
	if (__builtin_expect(len > 4, 0)) {
		uint64_t v = (x & 0x0F0F0F0F0F0F0F0Full) << (64 - 8 * len);
		v = (v * 2561) >> 8;
		v &= 0x00FF00FF00FF00FFull;
		v = (v * 6553601) >> 16;
		v &= 0x0000FFFF0000FFFFull;
		v = (v * 42949672960001ull) >> 32;
		return (unsigned)v;
	}
	uint32_t v = ((uint32_t)x & 0x0F0F0F0Fu) << (32 - 8 * len);
	v = (v * 10 + (v >> 8)) & 0x00FF00FFu;
	v = (v * 100 + (v >> 16)) & 0x3FFFu;
	return v;
}

struct Sim {
	uint64_t *g;
	int nw;
	int rs;
	int flowCount;
	int waterCount;

	uint64_t *W(int y) const { return g + (size_t)y * rs; }
	uint64_t *F(int y) const { return g + (size_t)y * rs + nw; }

	inline uint64_t stopword(int y, int w) const {
		const uint64_t *r = g + (size_t)y * rs;
		return r[w] | r[w + nw] | ~r[w + rs];
	}

	int scan_left(int y, int x) const {
		int w = x >> 6;
		uint64_t m = stopword(y, w) & ((1ull << (x & 63)) - 1);
		while (!m) {
			--w;
			m = stopword(y, w);
		}
		return (w << 6) + 63 - __builtin_clzll(m);
	}

	int scan_right(int y, int x) const {
		int w = x >> 6;
		int b = (x & 63) + 1;
		uint64_t m = b == 64 ? 0 : stopword(y, w) & (~0ull << b);
		while (!m) {
			++w;
			m = stopword(y, w);
		}
		return (w << 6) + __builtin_ctzll(m);
	}

	bool fall(int x, int y);
};

bool Sim::fall(int x, int y0) {
	int y = y0;
	for (;;) {
		uint64_t *r = g + (size_t)y * rs;
		uint64_t bit = 1ull << (x & 63);
		int w = x >> 6;
		if ((r[w] | r[w + nw]) & bit) {
			if (!(r[w] & bit)) return true;
			break;
		}
		r[w + nw] |= bit;
		flowCount++;
		y++;
	}
	for (y--; y >= y0; y--) {
		uint64_t *Wr = W(y), *Fr = F(y);
		{
			int pl = scan_left(y, x), pr = scan_right(y, x);
			if (test(Wr, pl) & test(Wr, pr)) {
				set_range2(Wr, Fr, pl + 1, pr - 1, flowCount);
				waterCount += pr - pl - 1;
				continue;
			}
		}
		int l = x, r = x;
		bool leak = false;
		for (;;) {
			int p = scan_left(y, l);
			if (p + 1 < l) {
				set_range(Fr, p + 1, l - 1);
				flowCount += l - 1 - p;
			}
			l = p + 1;
			if (test(Wr, p)) break;
			if (test(Fr, p)) { leak = true; break; }
			setb(Fr, p);
			flowCount++;
			l = p;
			if (test(F(y + 1), p) || fall(p, y + 1)) { leak = true; break; }
		}
		for (;;) {
			int p = scan_right(y, r);
			if (p - 1 > r) {
				set_range(Fr, r + 1, p - 1);
				flowCount += p - 1 - r;
			}
			r = p - 1;
			if (test(Wr, p)) break;
			if (test(Fr, p)) { leak = true; break; }
			setb(Fr, p);
			flowCount++;
			r = p;
			if (test(F(y + 1), p) || fall(p, y + 1)) { leak = true; break; }
		}
		if (leak) return true;
		set_range(Wr, l, r);
		waterCount += r - l + 1;
	}
	return false;
}

}

void day17(input_t input) {
	std::unique_ptr<uint32_t[]> posbuf(new uint32_t[(size_t)input.len / 2 + 64]);
	uint32_t *const pos = posbuf.get();
	uint32_t *pp = pos;
	const uint8_t *s = (const uint8_t *)input.s;
	size_t len = (size_t)input.len;
	uint64_t carry = 0;
	const uint8x16_t c0 = vdupq_n_u8('0'), c10 = vdupq_n_u8(10);
	for (size_t off = 0; off < len; off += 16) {
		uint8x16_t v = vld1q_u8(s + off);
		uint8x16_t isd = vcltq_u8(vsubq_u8(v, c0), c10);
		uint64_t m = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(isd), 4)), 0) & 0x1111111111111111ull;
		if (len - off < 16) m &= (1ull << (4 * (len - off))) - 1;
		uint64_t st = m & ~((m << 4) | carry);
		carry = m >> 60;
		int cnt = __builtin_popcountll(st);
		uint32_t o = (uint32_t)off;
		pp[0] = o + (__builtin_ctzll(st) >> 2);
		st &= st - 1;
		pp[1] = o + (__builtin_ctzll(st) >> 2);
		st &= st - 1;
		pp[2] = o + (__builtin_ctzll(st) >> 2);
		st &= st - 1;
		pp[3] = o + (__builtin_ctzll(st) >> 2);
		st &= st - 1;
		if (__builtin_expect(cnt > 4, 0)) {
			uint32_t *q = pp + 4;
			while (st) {
				*q++ = o + (__builtin_ctzll(st) >> 2);
				st &= st - 1;
			}
		}
		pp += cnt;
	}
	size_t nnum = (pp - pos) / 3;
	std::unique_ptr<Vein[]> Vbuf(new Vein[2 * nnum + 2]);
	Vein *VV = Vbuf.get(), *VH = VV + nnum + 1;
	size_t nv = 0, nh = 0;
	int minx = INT_MAX, maxx = INT_MIN, miny = INT_MAX, maxy = INT_MIN;
	for (size_t i = 0; i < nnum; i++) {
		const uint32_t *q = pos + 3 * i;
		bool xfirst = s[q[0] - 2] == 'x';
		unsigned a = parse_num((const char *)s + q[0]);
		unsigned b0 = parse_num((const char *)s + q[1]);
		unsigned b1 = parse_num((const char *)s + q[2]);
		Vein vn{(uint16_t)a, (uint16_t)b0, (uint16_t)b1, xfirst};
		VV[nv] = vn;
		VH[nh] = vn;
		nv += xfirst;
		nh += !xfirst;
		int vx0 = xfirst ? a : b0, vx1 = xfirst ? a : b1;
		int vy0 = xfirst ? b0 : a, vy1 = xfirst ? b1 : a;
		minx = std::min(minx, vx0);
		maxx = std::max(maxx, vx1);
		miny = std::min(miny, vy0);
		maxy = std::max(maxy, vy1);
	}
	int ox = minx - 2;
	int width = (maxx + 3) - ox;
	int nw = (width + 63) >> 6;
	int rs = 2 * nw;
	int rows = maxy - miny + 3;
	size_t words = (size_t)rs * rows;
	std::vector<uint64_t> heap;
	uint64_t *g;
	if (words * 8 <= (1u << 18)) {
		g = (uint64_t *)__builtin_alloca(words * 8);
	} else {
		heap.resize(words);
		g = heap.data();
	}
	memset(g, 0, words * 8);
	for (int y = 0; y < rows - 1; y++) {
		uint64_t *F = g + (size_t)y * rs + nw;
		F[0] |= 1;
		F[nw - 1] |= ~0ull << ((width - 1) & 63);
	}
	memset(g + (size_t)(rows - 1) * rs + nw, 0xff, nw * 8);
	int oy = miny - 1;
	for (size_t i = 0; i < nv; i++) {
		const Vein &v = VV[i];
		int x = v.a - ox;
		uint64_t *p = g + (size_t)(v.b0 - oy) * rs + (x >> 6);
		uint64_t bit = 1ull << (x & 63);
		for (int y = v.b0; y <= v.b1; y++, p += rs) *p |= bit;
	}
	for (size_t i = 0; i < nh; i++) {
		const Vein &v = VH[i];
		set_range(g + (size_t)(v.a - oy) * rs, v.b0 - ox, v.b1 - ox);
	}

	Sim S{g, nw, rs, 0, 0};
	S.fall(500 - ox, 0);
	printf("Day 17 Part 1: %d\nDay 17 Part 2: %d\n", S.flowCount - 1, S.waterCount);
}
