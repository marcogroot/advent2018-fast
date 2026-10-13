#include "advent2018.h"
#include <arm_neon.h>

namespace {

constexpr int MAXC = 64;
constexpr int PAD = 32;
constexpr int MAXN = 1 << 15;
constexpr int MAXW = 256;

inline uint64_t movemask64(uint8x16_t m0, uint8x16_t m1, uint8x16_t m2, uint8x16_t m3) {
	const uint8x16_t bits = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
	uint8x16_t a = vpaddq_u8(vandq_u8(m0, bits), vandq_u8(m1, bits));
	uint8x16_t b = vpaddq_u8(vandq_u8(m2, bits), vandq_u8(m3, bits));
	uint8x16_t c = vpaddq_u8(a, b);
	c = vpaddq_u8(c, c);
	return vgetq_lane_u64(vreinterpretq_u64_u8(c), 0);
}

inline unsigned kidx(unsigned char c) {
	return (c >> 1) & 3;
}

struct EdgeTab {
	uint32_t t[4][4];
};

constexpr EdgeTab make_edge_tab() {
	EdgeTab E = { };
	for (int d = 0; d < 4; d++) {
		E.t[1][d] = (1u << 16) | ((((d + 3) & 3) | (d << 2) | (((d + 1) & 3) << 4)) << 8);
		E.t[3][d] = ((d ^ 1) * 0x15u) << 8;
		E.t[2][d] = ((d ^ 3) * 0x15u) << 8;
		E.t[0][d] = 0;
	}
	return E;
}

constexpr EdgeTab ETAB = make_edge_tab();

inline uint32_t edge(int v, unsigned k, int d, unsigned L) {
	return ((uint32_t)v << 17) | ETAB.t[k][d] | (L & 0xff);
}

constexpr int dyv[4] = { -1, 0, 1, 0 };
constexpr int dxv[4] = { 0, 1, 0, -1 };

}

void day13(input_t input) {
	const unsigned char *s = (const unsigned char *)input.s;
	int len = (int)input.len;
	int stride = (const char *)memchr(s, '\n', len) - (const char *)s + 1;
	int width = stride - 1;
	int height = (len + stride - 1) / stride;

	uint32_t nxt[MAXN * 4];
	uint32_t nyx[MAXN];
	uint16_t id[MAXW * MAXW];

	constexpr int DUMMY = MAXN - 1;
	uint32_t colprev[MAXW];
	for (int x = 0; x < width; x++) colprev[x] = ((uint32_t)DUMMY << 10) | (1u << 8);

	int cp[MAXC];
	int n = 0;
	int nn = 0;
	const uint64_t inv = (1ull << 40) / stride + 1;

	const uint8x16_t vs = vdupq_n_u8('/'), vb = vdupq_n_u8('\\'), vp = vdupq_n_u8('+');
	const uint8x16_t vl = vdupq_n_u8('<'), vg = vdupq_n_u8('>'), vu = vdupq_n_u8('^'), vv = vdupq_n_u8('v');

	int rw = DUMMY, rx = 0;
	unsigned rk = 1;
	for (int p0 = 0; p0 < len; p0 += 64) {
		uint8x16_t c[4], mn[4], mc[4];
		for (int k = 0; k < 4; k++) {
			c[k] = vld1q_u8(s + p0 + 16 * k);
			mn[k] = vorrq_u8(vorrq_u8(vceqq_u8(c[k], vs), vceqq_u8(c[k], vb)), vceqq_u8(c[k], vp));
			mc[k] = vorrq_u8(vorrq_u8(vceqq_u8(c[k], vl), vceqq_u8(c[k], vg)), vorrq_u8(vceqq_u8(c[k], vu), vceqq_u8(c[k], vv)));
		}
		uint64_t bn = movemask64(mn[0], mn[1], mn[2], mn[3]);
		uint64_t bc = movemask64(mc[0], mc[1], mc[2], mc[3]);
		int rem = len - p0;
		if (rem < 64) {
			uint64_t lim = (1ull << rem) - 1;
			bn &= lim;
			bc &= lim;
		}
		if (__builtin_expect(bc != 0, 0)) {
			do {
				int p = p0 + __builtin_ctzll(bc);
				bc &= bc - 1;
				if (n < MAXC) cp[n++] = p;
			} while (bc);
		}
		while (bn) {
			int p = p0 + __builtin_ctzll(bn);
			bn &= bn - 1;
			int y = (int)(((uint64_t)p * inv) >> 40);
			int x = p - y * stride;
			unsigned k = kidx(s[p]);
			int u = nn++;
			id[p] = u;
			nyx[u] = (y << 16) | x;
			unsigned hl = (unsigned)(x - rx);
			nxt[rw * 4 + 1] = edge(u, k, 1, hl);
			nxt[u * 4 + 3] = edge(rw, rk, 3, hl);
			rw = u;
			rx = x;
			rk = k;
			uint32_t cv = colprev[x];
			int v = cv >> 10;
			unsigned vl = (unsigned)(y - (int)(cv & 0xff));
			nxt[v * 4 + 2] = edge(u, k, 2, vl);
			nxt[u * 4 + 0] = edge(v, (cv >> 8) & 3, 0, vl);
			colprev[x] = ((uint32_t)u << 10) | (k << 8) | y;
		}
	}

	int slot[MAXC], st[MAXC], arr[MAXC];
	uint32_t ce[MAXC];
	alignas(16) int16_t ys[MAXC + PAD], xs[MAXC + PAD];
	int key[MAXC];

	for (int i = 0; i < n; i++) {
		int p = cp[i];
		unsigned char ch = s[p];
		int d = ch == '^' ? 0 : ch == '>' ? 1 : ch == 'v' ? 2 : 3;
		int dl = d == 0 ? -stride : d == 1 ? 1 : d == 2 ? stride : -1;
		int L = 0;
		do {
			p += dl;
			L++;
		} while (s[p] != '/' && s[p] != '\\' && s[p] != '+');
		ce[i] = edge(id[p], kidx(s[p]), d, L);
		slot[i] = d;
		st[i] = 0;
		arr[i] = L;
	}

	auto advance = [&](int i) {
		uint32_t e = ce[i];
		int t = st[i];
		int sl = (int)(e >> 17) * 4 + ((e >> (8 + 2 * t)) & 3);
		int tp = t == 2 ? 0 : t + 1;
		st[i] = (e & 0x10000) ? tp : t;
		uint32_t e2 = nxt[sl];
		slot[i] = sl;
		ce[i] = e2;
		arr[i] += e2 & 0xff;
	};
	auto locate = [&](int i, int T, int &y, int &x) {
		int r = arr[i] - T, d = slot[i] & 3, v = ce[i] >> 17;
		y = (int)(nyx[v] >> 16) - r * dyv[d];
		x = (int)(nyx[v] & 0xffff) - r * dxv[d];
	};

	int T = 0;
	bool have1 = false;
	int p1x = 0, p1y = 0;
	bool dead[MAXC];

	for (int i = 0; i < MAXC + PAD; i++) ys[i] = xs[i] = 10000;

	while (n > 1) {
		for (int i = 0; i < n; i++) {
			int y, x;
			locate(i, T, y, x);
			ys[i] = y;
			xs[i] = x;
		}
		int16x8_t vbest = vdupq_n_s16(INT16_MAX);
		for (int i = 0; i < n - 1; i++) {
			int16x8_t yi = vdupq_n_s16(ys[i]), xi = vdupq_n_s16(xs[i]);
			for (int j = i + 1; j < n; j += 8) {
				int16x8_t dd = vaddq_s16(vabdq_s16(vld1q_s16(ys + j), yi), vabdq_s16(vld1q_s16(xs + j), xi));
				vbest = vminq_s16(vbest, dd);
			}
		}
		int D = vminvq_s16(vbest);
		int k = (D - 1) >> 1;
		if (k > 0) {
			T += k;
			for (int i = 0; i < n; i++) {
				while (arr[i] <= T) advance(i);
			}
			continue;
		}
		for (int i = 0; i < n; i++) key[i] = (ys[i] << 16) | xs[i];
		for (int i = 1; i < n; i++) {
			for (int j = i; j > 0 && key[j - 1] > key[j]; j--) {
				std::swap(key[j - 1], key[j]);
				std::swap(slot[j - 1], slot[j]);
				std::swap(st[j - 1], st[j]);
				std::swap(arr[j - 1], arr[j]);
				std::swap(ce[j - 1], ce[j]);
			}
		}
		T++;
		for (int i = 0; i < n; i++) dead[i] = false;
		for (int i = 0; i < n; i++) {
			if (dead[i]) continue;
			if (arr[i] == T) advance(i);
			int y, x;
			locate(i, T, y, x);
			int kk = (y << 16) | x;
			key[i] = kk;
			for (int j = 0; j < n; j++) {
				if (j != i && !dead[j] && key[j] == kk) {
					dead[i] = dead[j] = true;
					if (!have1) {
						have1 = true;
						p1y = y;
						p1x = x;
					}
					break;
				}
			}
		}
		int m = 0;
		for (int i = 0; i < n; i++) {
			if (dead[i]) continue;
			slot[m] = slot[i];
			st[m] = st[i];
			arr[m] = arr[i];
			ce[m] = ce[i];
			m++;
		}
		for (int i = m; i < n; i++) ys[i] = xs[i] = 10000;
		n = m;
	}

	int p2x = 0, p2y = 0;
	if (n > 0) locate(0, T, p2y, p2x);
	printf("Day 13 Part 1: %d,%d\nDay 13 Part 2: %d,%d\n", p1x, p1y, p2x, p2y);
}
