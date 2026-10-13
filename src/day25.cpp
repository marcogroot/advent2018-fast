#include "advent2018.h"
#include <arm_neon.h>
#include <memory>

namespace {

struct CompactTable {
	uint8_t t[256][8];
	constexpr CompactTable() : t() {
		for (int m = 0; m < 256; m++) {
			int k = 0;
			for (int b = 0; b < 8; b++) {
				if (m & (1 << b)) {
					t[m][k++] = b;
				}
			}
			for (; k < 8; k++) {
				t[m][k] = 0;
			}
		}
	}
};

constexpr CompactTable compact_table_holder;
constexpr auto &compact_table = compact_table_holder.t;

struct DisjointSet {
	std::vector<uint16_t> parent;
	uint32_t trees;

	DisjointSet(uint32_t n) : parent(n), trees(n) {
		for (uint32_t i = 0; i < n; i++) {
			parent[i] = i;
		}
	}

	uint32_t find(uint32_t x) {
		while (parent[x] != x) {
			uint32_t p = parent[x];
			uint32_t g = parent[p];
			parent[x] = g;
			x = g;
		}
		return x;
	}

	void join(uint32_t x, uint32_t y) {
		x = find(x);
		y = find(y);
		if (x != y) {
			trees--;
			if (x < y) {
				parent[y] = x;
			} else {
				parent[x] = y;
			}
		}
	}
};


static uint32_t solve_sorted(const int8_t *raw, uint32_t n) {
	int minx = 127, maxx = -128, miny = 127, maxy = -128;
	for (uint32_t i = 0; i < n; i++) {
		int x = raw[i * 4], y = raw[i * 4 + 1];
		minx = std::min(minx, x);
		maxx = std::max(maxx, x);
		miny = std::min(miny, y);
		maxy = std::max(maxy, y);
	}
	int nx = maxx - minx + 1, ny = maxy - miny + 1;
	int nb = nx * ny;
	std::vector<uint32_t> bstart(nb + 1, 0);
	std::vector<uint16_t> bidx(n);
	for (uint32_t i = 0; i < n; i++) {
		int b = (raw[i * 4] - minx) * ny + (raw[i * 4 + 1] - miny);
		bidx[i] = b;
		bstart[b + 1]++;
	}
	for (int b = 0; b < nb; b++) {
		bstart[b + 1] += bstart[b];
	}

	size_t padded = n + 32;
	std::vector<int8_t> soa(padded * 4, 0);
	int8_t *X = &soa[0], *Y = X + padded, *Z = Y + padded, *T = Z + padded;
	{
		std::vector<uint32_t> pos(bstart.begin(), bstart.end() - 1);
		for (uint32_t i = 0; i < n; i++) {
			const int8_t *p = &raw[i * 4];
			uint32_t k = pos[bidx[i]]++;
			X[k] = p[0];
			Y[k] = p[1];
			Z[k] = p[2];
			T[k] = p[3];
		}
	}

	DisjointSet S(n);
	const uint8x16_t three = vdupq_n_u8(3);
	static const uint8_t iota_bytes[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
	const uint8x16_t iota = vld1q_u8(iota_bytes);

	for (int bx = 0; bx < nx; bx++) {
		for (int by = 0; by < ny; by++) {
			int b = bx * ny + by;
			uint32_t i0 = bstart[b], i1 = bstart[b + 1];
			if (i0 == i1) continue;
			uint32_t rlo[4], rhi[4];
			int nr = 0;
			for (int dx = 0; dx <= 3 && bx - dx >= 0; dx++) {
				int r = 3 - dx;
				int ylo = std::max(by - r, 0);
				int yhi = dx == 0 ? by - 1 : std::min(by + r, ny - 1);
				if (ylo > yhi) continue;
				int row = (bx - dx) * ny;
				uint32_t a = bstart[row + ylo], e = bstart[row + yhi + 1];
				if (a < e) {
					rlo[nr] = a;
					rhi[nr] = e;
					nr++;
				}
			}
			for (uint32_t i = i0; i < i1; i++) {
				int8x16_t xi = vdupq_n_s8(X[i]);
				int8x16_t yi = vdupq_n_s8(Y[i]);
				int8x16_t zi = vdupq_n_s8(Z[i]);
				int8x16_t ti = vdupq_n_s8(T[i]);
				for (int r = 0; r <= nr; r++) {
					uint32_t lo = r < nr ? rlo[r] : i0;
					uint32_t hi = r < nr ? rhi[r] : i;
					for (uint32_t j = lo; j < hi; j += 16) {
						uint8x16_t d = vreinterpretq_u8_s8(vabdq_s8(xi, vld1q_s8(X + j)));
						d = vreinterpretq_u8_s8(vabaq_s8(vreinterpretq_s8_u8(d), yi, vld1q_s8(Y + j)));
						d = vreinterpretq_u8_s8(vabaq_s8(vreinterpretq_s8_u8(d), zi, vld1q_s8(Z + j)));
						d = vreinterpretq_u8_s8(vabaq_s8(vreinterpretq_s8_u8(d), ti, vld1q_s8(T + j)));
						uint8x16_t c = vandq_u8(vcleq_u8(d, three), vcltq_u8(iota, vdupq_n_u8(std::min(hi - j, 16u))));
						uint64_t m = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(c), 4)), 0);
						m &= 0x1111111111111111ULL;
						while (m) {
							S.join(i, j + (__builtin_ctzll(m) >> 2));
							m &= m - 1;
						}
					}
				}
			}
		}
	}

	return S.trees;
}

template <int L>
static bool solve_grid(const int8_t *raw, uint32_t n, uint32_t &result) {
	constexpr int GY = 19 + 6, GX = 19 + 3;
	constexpr int ncells = GX * GY;
	constexpr int CAP = 16 * L;
	struct Cell {
		int8_t z[CAP];
		int8_t t[CAP];
	};
	std::unique_ptr<Cell[]> cells(new Cell[ncells]);
	memset(cells.get(), 100, sizeof(Cell) * ncells);
	uint16_t cnt[ncells] = {};
	std::unique_ptr<uint16_t[]> parent(new uint16_t[ncells * CAP]);
	uint16_t *P = parent.get();

	uint32_t maxk = 0;
	Cell *CW = cells.get();
	for (uint32_t i = 0; i < n; i++) {
		uint32_t w;
		memcpy(&w, raw + i * 4, 4);
		int8_t x = (int8_t)w, y = (int8_t)(w >> 8), z = (int8_t)(w >> 16), t = (int8_t)(w >> 24);
		uint32_t c = (x + 12) * GY + (y + 12);
		uint32_t k = cnt[c]++;
		maxk = std::max(maxk, k);
		k = std::min(k, (uint32_t)CAP - 1);
		CW[c].z[k] = z;
		CW[c].t[k] = t;
		P[c * CAP + k] = c * CAP + k;
	}
	if (maxk >= CAP) {
		return false;
	}

	auto find = [P](uint32_t x) {
		while (P[x] != x) {
			uint32_t g = P[P[x]];
			P[x] = g;
			x = g;
		}
		return x;
	};

	static const uint8_t iota_bytes[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
	const uint8x16_t iota = vld1q_u8(iota_bytes);
	const uint8x16_t one = vdupq_n_u8(1), two = vdupq_n_u8(2), three = vdupq_n_u8(3);

	static const int8_t nb[12][2] = {
		{0, 1}, {0, 2}, {0, 3}, {1, -2}, {1, -1}, {1, 0}, {1, 1}, {1, 2},
		{2, -1}, {2, 0}, {2, 1}, {3, 0},
	};
	int delta[16];
	for (int b = 0; b < 8; b++) {
		delta[b] = -(nb[7 - b][0] * GY + nb[7 - b][1]);
	}
	for (int b = 0; b < 4; b++) {
		delta[8 + b] = -(nb[11 - b][0] * GY + nb[11 - b][1]);
	}
	delta[12] = 0;

	struct Rec {
		uint8x16_t v1[L], v2[L];
		uint32_t self;
	};
	std::unique_ptr<Rec[]> recs(new Rec[n + 1]);
	Rec *R = recs.get();
	size_t nr = 0;
	const Cell *C = cells.get();

	uint8_t ltk[17][16];
	for (int k = 0; k <= 16; k++) {
		for (int j = 0; j < 16; j++) {
			ltk[k][j] = j < k;
		}
	}

	for (int c = 3 * GY; c < ncells; c++) {
		uint32_t m_cnt = cnt[c];
		if (!m_cnt) continue;
		const Cell *p0 = C + c, *p1 = p0 - GY, *p2 = p1 - GY, *p3 = p2 - GY;
		for (uint32_t k = 0; k < m_cnt; k++) {
			uint32_t self = c * CAP + k;
			int8x16_t zi = vld1q_dup_s8(&p0->z[k]);
			int8x16_t ti = vld1q_dup_s8(&p0->t[k]);
			uint8x16_t acc = vdupq_n_u8(0);
			for (int l = 0; l < L; l++) {
				auto D = [&](const Cell *q) {
					return vreinterpretq_u8_s8(vabaq_s8(vabdq_s8(zi, vld1q_s8(q->z + 16 * l)), ti, vld1q_s8(q->t + 16 * l)));
				};
				uint8x16_t v1 = vcleq_u8(D(p0 - 1), two);
				v1 = vsliq_n_u8(vcleq_u8(D(p0 - 2), one), v1, 1);
				v1 = vsliq_n_u8(vceqzq_u8(D(p0 - 3)), v1, 1);
				v1 = vsliq_n_u8(vceqzq_u8(D(p1 + 2)), v1, 1);
				v1 = vsliq_n_u8(vcleq_u8(D(p1 + 1), one), v1, 1);
				v1 = vsliq_n_u8(vcleq_u8(D(p1), two), v1, 1);
				v1 = vsliq_n_u8(vcleq_u8(D(p1 - 1), one), v1, 1);
				v1 = vsliq_n_u8(vceqzq_u8(D(p1 - 2)), v1, 1);
				int kl = L == 1 ? (int)k : std::min(std::max((int)k - 16 * l, 0), 16);
				uint8x16_t lm = vld1q_u8(ltk[kl]);
				uint8x16_t v2 = vandq_u8(vcleq_u8(D(p0), three), lm);
				v2 = vsliq_n_u8(vceqzq_u8(D(p2 + 1)), v2, 1);
				v2 = vsliq_n_u8(vcleq_u8(D(p2), one), v2, 1);
				v2 = vsliq_n_u8(vceqzq_u8(D(p2 - 1)), v2, 1);
				v2 = vsliq_n_u8(vceqzq_u8(D(p3)), v2, 1);
				R[nr].v1[l] = v1;
				R[nr].v2[l] = v2;
				acc = vorrq_u8(acc, vorrq_u8(v1, v2));
			}
			R[nr].self = self;
			nr += vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(vtstq_u8(acc, acc)), 4)), 0) != 0;
		}
	}

	uint32_t trees = n;
	const uint64_t lanebit = 0x1111111111111111ULL;
	for (size_t e = 0; e < nr; e++) {
		uint32_t self = R[e].self;
		uint32_t c = self / CAP;
		uint32_t r = self;
		for (int l = 0; l < L; l++) {
			uint8x16_t v1 = R[e].v1[l], v2 = R[e].v2[l];
			uint8x16_t o = vorrq_u8(v1, v2);
			uint64_t lanes = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(vtstq_u8(o, o)), 4)), 0) & lanebit;
			if (!lanes) continue;
			uint16_t bms[16];
			vst1q_u8((uint8_t *)bms, vzip1q_u8(v1, v2));
			vst1q_u8((uint8_t *)bms + 16, vzip2q_u8(v1, v2));
			do {
				uint32_t j = __builtin_ctzll(lanes) >> 2;
				lanes &= lanes - 1;
				uint32_t bm = bms[j];
				do {
					uint32_t ry = find((c + delta[__builtin_ctz(bm)]) * CAP + 16 * l + j);
					bm &= bm - 1;
					if (ry != r) {
						trees--;
						if (ry < r) {
							P[r] = ry;
							r = ry;
						} else {
							P[ry] = r;
						}
					}
				} while (bm);
			} while (lanes);
		}
	}
	result = trees;
	return true;
}

static uint32_t solve(const int8_t *raw, uint32_t n) {
	uint32_t result;
	if (solve_grid<1>(raw, n, result) || solve_grid<2>(raw, n, result)) {
		return result;
	}
	return solve_sorted(raw, n);
}

}

void day25(input_t input) {
	const uint8_t *s = (const uint8_t *)input.s, *end = s + input.len;
	std::unique_ptr<int8_t[]> rawbuf(new int8_t[input.len + 64]);
	int8_t *raw = rawbuf.get();
	uint32_t nv = 0;

	{
		static const uint8_t bitw_bytes[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
		const uint8x16_t bitw = vld1q_u8(bitw_bytes);
		uint8x16_t prev = vdupq_n_u8(0);
		for (; s < end; s += 16) {
			uint8x16_t c = vld1q_u8(s);
			uint8x16_t isminus = vceqq_u8(vextq_u8(prev, c, 15), vdupq_n_u8('-'));
			prev = c;
			uint8x16_t dv = vsubq_u8(c, vdupq_n_u8('0'));
			uint8x16_t isdig = vcltq_u8(dv, vdupq_n_u8(10));
			int8x16_t val = vreinterpretq_s8_u8(dv);
			val = vbslq_s8(isminus, vnegq_s8(val), val);
			uint8x16_t mb = vandq_u8(isdig, bitw);
			uint32_t m0 = vaddv_u8(vget_low_u8(mb));
			uint32_t m1 = vaddv_u8(vget_high_u8(mb));
			int8x8_t lo = vtbl1_s8(vget_low_s8(val), vld1_s8((const int8_t *)compact_table[m0]));
			int8x8_t hi = vtbl1_s8(vget_high_s8(val), vld1_s8((const int8_t *)compact_table[m1]));
			vst1_s8(raw + nv, lo);
			nv += __builtin_popcount(m0);
			vst1_s8(raw + nv, hi);
			nv += __builtin_popcount(m1);
		}
	}

	uint32_t n = nv / 4;
	uint32_t trees = n ? solve(raw, n) : 0;
	printf("Day 25 Part 1: %u\n", trees);
}
