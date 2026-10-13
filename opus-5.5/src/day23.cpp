#include "advent2018.h"
#include <arm_neon.h>
#include <memory>

namespace {

using Range = std::pair<int64_t, int64_t>;

struct Region {
	std::array<Range, 4> R;

	int64_t distance_to_origin() const {
		Region Ra0 = *this, Ra1 = *this;
		bool bad0 = false, bad1 = false;
		for (size_t i = 0; i < 4; i++) {
			auto&& r = R[i];

			if (r.first & 1) {
				bad0 |= (r.first == r.second);
				Ra0.R[i].first++;
			} else {
				bad1 |= (r.first == r.second);
				Ra1.R[i].first++;
			}

			if (r.second & 1) {
				Ra0.R[i].second--;
			} else {
				Ra1.R[i].second--;
			}
		}

		auto best_dist = INT64_MAX;
		if (!bad0) best_dist = std::min(best_dist, Ra0.solve_distance());
		if (!bad1) best_dist = std::min(best_dist, Ra1.solve_distance());
		return best_dist;
	}

    private:
	int64_t solve_distance() const {
		auto H = R;

		for (int i = 0; i < 3; i++) {
			auto& r = H[i];
			if (llabs(r.second) < llabs(r.first)) {
				std::swap(r.first, r.second);
			}
		}

		auto sum = H[0].first + H[1].first + H[2].first;
		int64_t delta = 0;
		if (sum < H[3].first) {
			delta = H[3].first - sum;
		} else if (sum > H[3].second) {
			delta = H[3].second - sum;
			H[3].first = H[3].second;
		} else {
			H[3].first = sum;
		}

		int64_t dist = 0;
		for (auto&& r : H) {
			dist = std::max(dist, int64_t(llabs(r.first)));
		}

		while (delta) {
			int64_t candidates = 0;
			for (int i = 0; i < 3; i++) {
				int64_t step = 0;
				if (delta > 0) {
					if (H[i].second <= 0) continue;
					auto slack = std::min( dist, H[i].second) - H[i].first;
					step = std::min(slack, delta);
				} else {
					if (H[i].second >= 0) continue;
					auto slack = std::max(-dist, H[i].second) - H[i].first;
					step = std::max(slack, delta);
				}
				H[i].first += step;
				delta -= step;
				if (H[i].first != H[i].second) {
					candidates++;
				}
			}
			if (delta) {
				if (!candidates) {
					return INT64_MAX;
				}
				dist += (((llabs(delta) / 2) + (candidates - 1)) / candidates) * 2;
			}
		}

		return dist;
	}
};

const uint8_t popcnt4[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

alignas(16) const uint8_t compress_tbl[16][16] = {
#define L0 0,1,2,3
#define L1 4,5,6,7
#define L2 8,9,10,11
#define L3 12,13,14,15
#define Z 255,255,255,255
	{ Z, Z, Z, Z },
	{ L0, Z, Z, Z },
	{ L1, Z, Z, Z },
	{ L0, L1, Z, Z },
	{ L2, Z, Z, Z },
	{ L0, L2, Z, Z },
	{ L1, L2, Z, Z },
	{ L0, L1, L2, Z },
	{ L3, Z, Z, Z },
	{ L0, L3, Z, Z },
	{ L1, L3, Z, Z },
	{ L0, L1, L3, Z },
	{ L2, L3, Z, Z },
	{ L0, L2, L3, Z },
	{ L1, L2, L3, Z },
	{ L0, L1, L2, L3 },
#undef L0
#undef L1
#undef L2
#undef L3
#undef Z
};

template <typename T>
struct Solver {
	const T *lo[4];
	const T *hi[4];
	T *vals[4];
	T *tmp;
	int64_t bias[4];
	T A[4], B[4];
	size_t f[4], e[4];
	std::vector<std::unique_ptr<uint8_t[]>> chunks;
	size_t npad;
	size_t best_intersection = 0;
	int64_t best_distance = INT64_MAX;

	static constexpr int CHUNK_DEPTHS = 32;

	uint8_t *buf(int depth, int side) {
		size_t c = size_t(depth) / CHUNK_DEPTHS;
		while (chunks.size() <= c) chunks.emplace_back(new uint8_t[CHUNK_DEPTHS * 2 * npad]);
		return chunks[c].get() + (size_t(depth % CHUNK_DEPTHS) * 2 + side) * npad;
	}

	void split_masks(const uint8_t *mask, const T *L, const T *H, T th, uint8_t *lowm, uint8_t *highm,
			size_t &valid_low, size_t &valid_high) {
		if constexpr (sizeof(T) == 4) {
			uint32x4_t t = vdupq_n_u32(th);
			uint16x8_t cl = vdupq_n_u16(0), ch = cl;
			const size_t np = npad;
			for (size_t k0 = 0; k0 < np; k0 += 16 * 255) {
				size_t kend = std::min(np, k0 + 16 * 255);
				uint8x16_t al = vdupq_n_u8(0), ah = al;
				for (size_t k = k0; k < kend; k += 16) {
					uint8x16_t m = vld1q_u8(mask + k);
					uint32x4_t a0 = vcltq_u32(vld1q_u32(L + k), t);
					uint32x4_t a1 = vcltq_u32(vld1q_u32(L + k + 4), t);
					uint32x4_t a2 = vcltq_u32(vld1q_u32(L + k + 8), t);
					uint32x4_t a3 = vcltq_u32(vld1q_u32(L + k + 12), t);
					uint32x4_t b0 = vcgtq_u32(vld1q_u32(H + k), t);
					uint32x4_t b1 = vcgtq_u32(vld1q_u32(H + k + 4), t);
					uint32x4_t b2 = vcgtq_u32(vld1q_u32(H + k + 8), t);
					uint32x4_t b3 = vcgtq_u32(vld1q_u32(H + k + 12), t);
					uint16x8_t a01 = vuzp1q_u16(vreinterpretq_u16_u32(a0), vreinterpretq_u16_u32(a1));
					uint16x8_t a23 = vuzp1q_u16(vreinterpretq_u16_u32(a2), vreinterpretq_u16_u32(a3));
					uint16x8_t b01 = vuzp1q_u16(vreinterpretq_u16_u32(b0), vreinterpretq_u16_u32(b1));
					uint16x8_t b23 = vuzp1q_u16(vreinterpretq_u16_u32(b2), vreinterpretq_u16_u32(b3));
					uint8x16_t l = vandq_u8(m, vuzp1q_u8(vreinterpretq_u8_u16(a01), vreinterpretq_u8_u16(a23)));
					uint8x16_t h = vandq_u8(m, vuzp1q_u8(vreinterpretq_u8_u16(b01), vreinterpretq_u8_u16(b23)));
					vst1q_u8(lowm + k, l);
					vst1q_u8(highm + k, h);
					al = vsubq_u8(al, l);
					ah = vsubq_u8(ah, h);
				}
				cl = vpadalq_u8(cl, al);
				ch = vpadalq_u8(ch, ah);
			}
			valid_low = vaddlvq_u16(cl);
			valid_high = vaddlvq_u16(ch);
		} else {
			valid_low = valid_high = 0;
			for (size_t k = 0; k < npad; k++) {
				uint8_t m = mask[k];
				uint8_t l = m & -uint8_t(L[k] < th);
				uint8_t h = m & -uint8_t(H[k] > th);
				lowm[k] = l;
				highm[k] = h;
				valid_low += l & 1;
				valid_high += h & 1;
			}
		}
	}

	static T med3(T a, T b, T c) {
		return std::max(std::min(a, b), std::min(std::max(a, b), c));
	}

	void recur(int axis, size_t valid, const uint8_t *mask, int depth) {
		bool ok = false;
		for (int i = 0; i < 4; i++, axis = (axis + 1) & 3) {
			ok = f[axis] < e[axis];
			if (ok) break;
		}

		if (!ok) {
			Region region;
			for (int i = 0; i < 4; i++) {
				region.R[i] = { int64_t(A[i]) + bias[i], int64_t(B[i]) + bias[i] - 1 };
			}
			auto dist = region.distance_to_origin();
			if (dist != INT64_MAX) {
				if (best_intersection < valid) {
					best_intersection = valid;
					best_distance = dist;
				} else {
					best_distance = std::min(best_distance, dist);
				}
			}
			return;
		}

		size_t f0 = f[axis], e0 = e[axis];
		T A0 = A[axis], B0 = B[axis];
		T *v = vals[axis];
		size_t n = e0 - f0;
		T th;
		if (n >= 64) {
			size_t q = n / 9;
			th = med3(med3(v[f0], v[f0 + q], v[f0 + 2 * q]),
			          med3(v[f0 + 3 * q], v[f0 + 4 * q], v[f0 + 5 * q]),
			          med3(v[f0 + 6 * q], v[f0 + 7 * q], v[f0 + 8 * q]));
		} else {
			th = med3(v[f0], v[f0 + n / 2], v[e0 - 1]);
		}
		size_t nl = 0, nh = 0;
		T *tmpL = tmp, *tmpH = tmp + n + 8;
		size_t i = f0;
		if constexpr (sizeof(T) == 4) {
			uint32x4_t t = vdupq_n_u32(th);
			const uint32x4_t bitw = { 1, 2, 4, 8 };
			const uint32x4_t bitw2 = { 16, 32, 64, 128 };
			for (; i + 4 <= e0; i += 4) {
				uint32x4_t x = vld1q_u32(v + i);
				unsigned b = vaddvq_u32(vorrq_u32(vandq_u32(vcltq_u32(x, t), bitw), vandq_u32(vcgtq_u32(x, t), bitw2)));
				unsigned bl = b & 15, bh = b >> 4;
				uint8x16_t xb = vreinterpretq_u8_u32(x);
				vst1q_u32(tmpL + nl, vreinterpretq_u32_u8(vqtbl1q_u8(xb, vld1q_u8(compress_tbl[bl]))));
				vst1q_u32(tmpH + nh, vreinterpretq_u32_u8(vqtbl1q_u8(xb, vld1q_u8(compress_tbl[bh]))));
				nl += popcnt4[bl];
				nh += popcnt4[bh];
			}
		}
		for (; i < e0; i++) {
			T x = v[i];
			tmpL[nl] = x;
			tmpH[nh] = x;
			nl += x < th;
			nh += x > th;
		}
		memcpy(v + f0, tmpL, nl * sizeof(T));
		memcpy(v + e0 - nh, tmpH, nh * sizeof(T));

		uint8_t *lowm = buf(depth + 1, 0);
		uint8_t *highm = buf(depth + 1, 1);
		const T *L = lo[axis], *H = hi[axis];
		size_t valid_low, valid_high;
		split_masks(mask, L, H, th, lowm, highm, valid_low, valid_high);

		int next = (axis + 1) & 3;
		auto go_low = [&]() {
			B[axis] = th; e[axis] = f0 + nl;
			recur(next, valid_low, lowm, depth + 1);
			B[axis] = B0; e[axis] = e0;
		};
		auto go_high = [&]() {
			A[axis] = th; f[axis] = e0 - nh;
			recur(next, valid_high, highm, depth + 1);
			A[axis] = A0; f[axis] = f0;
		};
		if (valid_low > valid_high) {
			if (valid_low >= best_intersection) go_low();
			if (valid_high >= best_intersection) go_high();
		} else {
			if (valid_high >= best_intersection) go_high();
			if (valid_low >= best_intersection) go_low();
		}
	}
};

template <typename T>
int64_t solve_part2(const int64_t *const *lo64, const int64_t *const *hi64, const int64_t *mn, const int64_t *mx, size_t N) {
	size_t npad = (N + 15) & ~size_t(15);
	size_t M = 2 * N;
	std::unique_ptr<T[]> storage(new T[8 * npad + 4 * (M + 1) + 2 * (M + 9)]);
	T *store = storage.get();
	Solver<T> S;
	S.npad = npad;
	S.tmp = store + 8 * npad + 4 * (M + 1);
	for (int a = 0; a < 4; a++) {
		T *lo = store + 2 * a * npad, *hi = lo + npad;
		T *v = store + 8 * npad + a * (M + 1);
		int64_t m = mn[a];
		const int64_t *L = lo64[a], *H = hi64[a];
		for (size_t i = 0; i < N; i++) {
			lo[i] = T(L[i] - m);
			hi[i] = T(H[i] - m);
		}
		for (size_t i = N; i < npad; i++) lo[i] = hi[i] = 0;
		T tA = 0, tB = T(mx[a] - m);
		memcpy(v, lo, N * sizeof(T));
		memcpy(v + N, hi, N * sizeof(T));
		v[M] = T(-m);
		size_t total = M + 1, cnt = 0, i = 0;
		T lim = tB - 1;
		if constexpr (sizeof(T) == 4) {
			uint32x4_t one = vdupq_n_u32(1), vl = vdupq_n_u32(lim);
			const uint32x4_t bitw = { 1, 2, 4, 8 };
			for (; i + 4 <= total; i += 4) {
				uint32x4_t x = vld1q_u32(v + i);
				unsigned b = vaddvq_u32(vandq_u32(vcltq_u32(vsubq_u32(x, one), vl), bitw));
				vst1q_u32(v + cnt, vreinterpretq_u32_u8(vqtbl1q_u8(vreinterpretq_u8_u32(x), vld1q_u8(compress_tbl[b]))));
				cnt += popcnt4[b];
			}
		}
		for (; i < total; i++) {
			T x = v[i];
			v[cnt] = x;
			cnt += T(x - 1) < lim;
		}
		S.lo[a] = lo; S.hi[a] = hi; S.vals[a] = v;
		S.bias[a] = m;
		S.A[a] = tA; S.B[a] = tB;
		S.f[a] = 0; S.e[a] = cnt;
	}
	uint8_t *m0 = S.buf(0, 0);
	memset(m0, 0xFF, N);
	memset(m0 + N, 0, npad - N);
	S.recur(0, N, m0, 0);
	return S.best_distance;
}

inline int64_t parse_digits(const char *p) {
	static const uint8_t iota_tbl[16] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
	static const uint8_t m1_tbl[16] = { 10,1,10,1,10,1,10,1,10,1,10,1,10,1,10,1 };
	static const uint16_t m2_tbl[8] = { 100,1,100,1,100,1,100,1 };
	static const uint32_t m3_tbl[4] = { 10000,1,10000,1 };
	uint8x16_t d = vsubq_u8(vld1q_u8((const uint8_t *)p), vdupq_n_u8('0'));
	uint8x16_t isdig = vcltq_u8(d, vdupq_n_u8(10));
	uint64_t nib = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(isdig), 4)), 0);
	unsigned len = __builtin_ctzll(~nib) >> 2;
	uint8x16_t ix = vaddq_u8(vld1q_u8(iota_tbl), vdupq_n_u8(uint8_t(len - 16)));
	uint8x16_t a = vqtbl1q_u8(d, ix);
	uint16x8_t b = vpaddlq_u8(vmulq_u8(a, vld1q_u8(m1_tbl)));
	uint32x4_t c = vpaddlq_u16(vmulq_u16(b, vld1q_u16(m2_tbl)));
	uint64x2_t e = vpaddlq_u32(vmulq_u32(c, vld1q_u32(m3_tbl)));
	int64_t n = int64_t(vgetq_lane_u64(e, 0) * 100000000ull + vgetq_lane_u64(e, 1));
	return p[-1] == '-' ? -n : n;
}
}

void day23(input_t input) {
	const char *base = input.s;
	size_t len = input.len;
	std::unique_ptr<uint32_t[]> starts(new uint32_t[len / 2 + 128]);
	uint32_t *sp = starts.get();
	size_t cnt = 0;
	uint64_t carry = 0;
	const uint8x16_t bitw = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
	const uint8x16_t zero = vdupq_n_u8('0'), ten = vdupq_n_u8(10);
	for (size_t o = 0; o < len; o += 64) {
		const uint8_t *q = (const uint8_t *)base + o;
		uint8x16_t c0 = vandq_u8(vcltq_u8(vsubq_u8(vld1q_u8(q), zero), ten), bitw);
		uint8x16_t c1 = vandq_u8(vcltq_u8(vsubq_u8(vld1q_u8(q + 16), zero), ten), bitw);
		uint8x16_t c2 = vandq_u8(vcltq_u8(vsubq_u8(vld1q_u8(q + 32), zero), ten), bitw);
		uint8x16_t c3 = vandq_u8(vcltq_u8(vsubq_u8(vld1q_u8(q + 48), zero), ten), bitw);
		uint8x16_t s01 = vpaddq_u8(c0, c1), s23 = vpaddq_u8(c2, c3);
		uint8x16_t s = vpaddq_u8(s01, s23);
		s = vpaddq_u8(s, s);
		uint64_t D = vgetq_lane_u64(vreinterpretq_u64_u8(s), 0);
		uint64_t S = D & ~((D << 1) | carry);
		carry = D >> 63;
		unsigned nb = __builtin_popcountll(S);
		uint32_t *w = sp + cnt;
		uint32_t oo = uint32_t(o);
		for (int j = 0; j < 8; j++) {
			w[j] = oo + __builtin_ctzll(S);
			S &= S - 1;
		}
		if (__builtin_expect(nb > 8, 0)) {
			for (unsigned j = 8; j < nb; j++) {
				w[j] = oo + __builtin_ctzll(S);
				S &= S - 1;
			}
		}
		cnt += nb;
	}
	while (cnt && sp[cnt - 1] >= len) cnt--;
	size_t N = cnt / 4;
	size_t NP = (N + 7) & ~size_t(7);
	std::unique_ptr<int64_t[]> soa(new int64_t[13 * NP]);
	int64_t *X = soa.get(), *Y = X + NP, *Z = Y + NP, *R = Z + NP;
	for (size_t i = 0; i < N; i++) {
		X[i] = parse_digits(base + sp[4 * i]);
		Y[i] = parse_digits(base + sp[4 * i + 1]);
		Z[i] = parse_digits(base + sp[4 * i + 2]);
		R[i] = parse_digits(base + sp[4 * i + 3]);
	}

	size_t big = 0;
	int64_t bigr = 0;
	for (size_t i = 0; i < N; i++) {
		if (R[i] > bigr) { bigr = R[i]; big = i; }
	}
	int32_t part1 = 0;
	if (N) {
		int64_t bx = X[big], by = Y[big], bz = Z[big];
		for (size_t i = 0; i < N; i++) {
			int64_t d = llabs(X[i] - bx) + llabs(Y[i] - by) + llabs(Z[i] - bz);
			part1 += d <= bigr;
		}
	}

	int64_t *lo64[4], *hi64[4];
	int64_t mn[4], mx[4];
	int64_t span = 0;
	for (int a = 0; a < 4; a++) {
		lo64[a] = R + NP + 2 * a * NP;
		hi64[a] = lo64[a] + NP;
		int64_t sx = a == 0 ? -1 : 1, sy = a == 1 ? -1 : 1, sz = a == 2 ? -1 : 1;
		int64_t lmn = 0, hmx = 0;
		for (size_t i = 0; i < N; i++) {
			int64_t v = sx * X[i] + sy * Y[i] + sz * Z[i];
			int64_t l = v - R[i], h = v + R[i] + 1;
			lo64[a][i] = l;
			hi64[a][i] = h;
			lmn = std::min(lmn, l);
			hmx = std::max(hmx, h);
		}
		mn[a] = lmn;
		mx[a] = hmx;
		span = std::max(span, std::max(-lmn, hmx));
	}
	int64_t part2 = span < (int64_t(1) << 30)
		? solve_part2<uint32_t>(lo64, hi64, mn, mx, N)
		: solve_part2<uint64_t>(lo64, hi64, mn, mx, N);

	printf("Day 23 Part 1: %d\nDay 23 Part 2: %ld\n", part1, part2);
}
