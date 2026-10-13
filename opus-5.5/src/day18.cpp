#include "advent2018.h"
#include <arm_neon.h>
#include <memory>

namespace {

#ifndef NEON_VECS
#define NEON_VECS 17
#endif

constexpr int ROWS = 50;
constexpr int G = NEON_VECS;
constexpr int SR = ROWS - 2 * G;
constexpr uint64_t COLMASK = (1ULL << 50) - 1;

struct alignas(16) State {
	uint64_t t[ROWS];
	uint64_t y[ROWS];
};

inline int slot_of_row(int r) {
	return r < G ? 2 * r : r < 2 * G ? 2 * (r - G) + 1 : r;
}

inline uint64_t mask64(uint8x16_t a, uint8x16_t b, uint8x16_t c, uint8x16_t d) {
	const uint8x16_t w = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
	a = vandq_u8(a, w); b = vandq_u8(b, w); c = vandq_u8(c, w); d = vandq_u8(d, w);
	uint8x16_t ab = vpaddq_u8(a, b), cd = vpaddq_u8(c, d);
	uint8x16_t abcd = vpaddq_u8(ab, cd);
	abcd = vpaddq_u8(abcd, abcd);
	return vgetq_lane_u64(vreinterpretq_u64_u8(abcd), 0);
}

void parse(const char *s, State &st) {
	const uint8x16_t tc = vdupq_n_u8('|'), yc = vdupq_n_u8('#');
	for (int r = 0; r < ROWS; r++, s += 51) {
		uint8x16_t a = vld1q_u8((const uint8_t *) s);
		uint8x16_t b = vld1q_u8((const uint8_t *) s + 16);
		uint8x16_t c = vld1q_u8((const uint8_t *) s + 32);
		uint8x16_t d = vld1q_u8((const uint8_t *) s + 48);
		int k = slot_of_row(r);
		st.t[k] = mask64(vceqq_u8(a, tc), vceqq_u8(b, tc), vceqq_u8(c, tc), vceqq_u8(d, tc)) & COLMASK;
		st.y[k] = mask64(vceqq_u8(a, yc), vceqq_u8(b, yc), vceqq_u8(c, yc), vceqq_u8(d, yc)) & COLMASK;
	}
}

struct HV {
	uint64x2_t t0, t1, y0, y1, rt, ry;
};

inline uint64x2_t maj(uint64x2_t a, uint64x2_t b, uint64x2_t c) {
	uint64x2_t m = veorq_u64(a, b);
	__asm__("bsl %0.16b, %1.16b, %2.16b" : "+w"(m) : "w"(c), "w"(a));
	return m;
}

inline void hsum(uint64x2_t r, uint64x2_t &h0, uint64x2_t &h1) {
	uint64x2_t L = vshlq_n_u64(r, 1), R = vshrq_n_u64(r, 1);
	h0 = veor3q_u64(L, r, R);
	h1 = maj(L, R, r);
}

inline HV hv_load(const State &s, int k) {
	HV h;
	h.rt = vld1q_u64(s.t + 2 * k);
	h.ry = vld1q_u64(s.y + 2 * k);
	hsum(h.rt, h.t0, h.t1);
	hsum(h.ry, h.y0, h.y1);
	return h;
}

struct HS {
	uint64_t t0, t1, y0, y1, rt, ry;
};

inline void hsum_s(uint64_t r, uint64_t &h0, uint64_t &h1) {
	uint64_t L = r << 1, R = r >> 1;
	h0 = L ^ r ^ R;
	h1 = (L & R) | (r & (L | R));
}

inline HS hs_load(const State &s, int i) {
	HS h;
	h.rt = s.t[i];
	h.ry = s.y[i];
	hsum_s(h.rt, h.t0, h.t1);
	hsum_s(h.ry, h.y0, h.y1);
	return h;
}

inline uint64_t mix(uint64x2_t at, uint64x2_t ay, uint64_t h) {
	uint64x2_t a = veorq_u64(at, vshlq_n_u64(ay, 11));
	h ^= vgetq_lane_u64(a, 0) * 0xD6E8FEB86659FD93ULL;
	h ^= (vgetq_lane_u64(a, 1) + (h >> 29)) * 0x9E3779B97F4A7C15ULL;
	return h ^ (h >> 31);
}

inline uint64_t step(const State &s, State &n) {
	const uint64x2_t M = vdupq_n_u64(COLMASK);
	const uint64x2_t Z = vdupq_n_u64(0);
	uint64x2_t acct = Z, accy = Z;
	uint64_t h = 0;

	HV w[3];
	HS sw[3];
	{
		HV last = hv_load(s, G - 1);
		w[2] = HV{vextq_u64(Z, last.t0, 1), vextq_u64(Z, last.t1, 1), vextq_u64(Z, last.y0, 1), vextq_u64(Z, last.y1, 1), Z, Z};
		sw[2] = HS{vgetq_lane_u64(last.t0, 1), vgetq_lane_u64(last.t1, 1), vgetq_lane_u64(last.y0, 1), vgetq_lane_u64(last.y1, 1), 0, 0};
	}
	w[0] = hv_load(s, 0);
	if (SR > 0) sw[0] = hs_load(s, 2 * G);
	const HS sfirst = SR > 0 ? sw[0] : HS{0, 0, 0, 0, 0, 0};

	auto neon = [&](int k, const HV &a, const HV &b, const HV &c) {
		uint64x2_t TC = b.rt, YC = b.ry;
		uint64x2_t to1 = vorrq_u64(vorrq_u64(a.t1, b.t1), c.t1);
		uint64x2_t to0 = vorrq_u64(vorrq_u64(a.t0, b.t0), c.t0);
		uint64x2_t tm1 = maj(a.t1, b.t1, c.t1);
		uint64x2_t tn3 = vandq_u64(vandq_u64(a.t0, b.t0), c.t0);
		uint64x2_t tg3 = vorrq_u64(vorrq_u64(tm1, tn3), vandq_u64(to1, to0));
		uint64x2_t tg1 = vorrq_u64(to1, to0);
		uint64x2_t yo1 = vorrq_u64(vorrq_u64(a.y1, b.y1), c.y1);
		uint64x2_t yo0 = vorrq_u64(vorrq_u64(a.y0, b.y0), c.y0);
		uint64x2_t ym1 = maj(a.y1, b.y1, c.y1);
		uint64x2_t yn3 = vandq_u64(vandq_u64(a.y0, b.y0), c.y0);
		uint64x2_t yg3 = vorrq_u64(vorrq_u64(ym1, yn3), vandq_u64(yo1, yo0));
		uint64x2_t yg2 = vorrq_u64(yo1, maj(a.y0, b.y0, c.y0));
		uint64x2_t open = vandq_u64(tg3, veorq_u64(vorrq_u64(TC, YC), M));
		uint64x2_t nt = vbcaxq_u64(open, TC, yg3);
		uint64x2_t ny = vorrq_u64(vandq_u64(TC, yg3), vandq_u64(YC, vandq_u64(tg1, yg2)));
		vst1q_u64(n.t + 2 * k, nt);
		vst1q_u64(n.y + 2 * k, ny);
		acct = vaddq_u64(acct, veorq_u64(nt, vshlq_n_u64(ny, 7)));
	};
	auto scalar = [&](int i, const HS &a, const HS &b, const HS &c) {
		uint64_t TC = b.rt, YC = b.ry;
		uint64_t to1 = a.t1 | b.t1 | c.t1;
		uint64_t to0 = a.t0 | b.t0 | c.t0;
		uint64_t tm1 = (a.t1 & b.t1) | (c.t1 & (a.t1 | b.t1));
		uint64_t tn3 = a.t0 & b.t0 & c.t0;
		uint64_t tg3 = tm1 | tn3 | (to1 & to0);
		uint64_t tg1 = to1 | to0;
		uint64_t yo1 = a.y1 | b.y1 | c.y1;
		uint64_t yo0 = a.y0 | b.y0 | c.y0;
		uint64_t ym1 = (a.y1 & b.y1) | (c.y1 & (a.y1 | b.y1));
		uint64_t yn3 = a.y0 & b.y0 & c.y0;
		uint64_t yg3 = ym1 | yn3 | (yo1 & yo0);
		uint64_t yg2 = yo1 | (a.y0 & b.y0) | (c.y0 & (a.y0 | b.y0));
		uint64_t nt = (tg3 & ~(TC | YC) & COLMASK) | (TC & ~yg3);
		uint64_t ny = (TC & yg3) | (YC & tg1 & yg2);
		n.t[i] = nt;
		n.y[i] = ny;
	};

	constexpr int NK = G > SR ? G : SR;
#pragma clang loop unroll(full)
	for (int k = 0; k < NK; k++) {
		if (k < G) {
			HV &nx = w[(k + 1) % 3];
			if (k + 1 < G) {
				nx = hv_load(s, k + 1);
			} else {
				HV f = hv_load(s, 0);
				uint64x2_t st0 = SR > 0 ? vdupq_n_u64(sfirst.t0) : Z;
				uint64x2_t st1 = SR > 0 ? vdupq_n_u64(sfirst.t1) : Z;
				uint64x2_t sy0 = SR > 0 ? vdupq_n_u64(sfirst.y0) : Z;
				uint64x2_t sy1 = SR > 0 ? vdupq_n_u64(sfirst.y1) : Z;
				nx = HV{vextq_u64(f.t0, st0, 1), vextq_u64(f.t1, st1, 1), vextq_u64(f.y0, sy0, 1), vextq_u64(f.y1, sy1, 1), Z, Z};
			}
			neon(k, w[(k + 2) % 3], w[k % 3], nx);
		}
		if (k < SR) {
			HS &nx = sw[(k + 1) % 3];
			if (k + 1 < SR) nx = hs_load(s, 2 * G + k + 1);
			else nx = HS{0, 0, 0, 0, 0, 0};
			scalar(2 * G + k, sw[(k + 2) % 3], sw[k % 3], nx);
		}
		__asm__ volatile("" ::: "memory");
	}
	return mix(acct, accy, h);
}

inline uint64_t hash_state(const State &s) {
	uint64x2_t acct = vdupq_n_u64(0), accy = vdupq_n_u64(0);
	for (int k = 0; k < G; k++)
		acct = vaddq_u64(acct, veorq_u64(vld1q_u64(s.t + 2 * k), vshlq_n_u64(vld1q_u64(s.y + 2 * k), 7)));
	uint64_t h = 0;
	return mix(acct, accy, h);
}

inline bool same(const State &a, const State &b) {
	uint64x2_t d = vdupq_n_u64(0);
	for (int i = 0; i < ROWS; i += 2) {
		d = vorrq_u64(d, veorq_u64(vld1q_u64(a.t + i), vld1q_u64(b.t + i)));
		d = vorrq_u64(d, veorq_u64(vld1q_u64(a.y + i), vld1q_u64(b.y + i)));
	}
	return (vgetq_lane_u64(d, 0) | vgetq_lane_u64(d, 1)) == 0;
}

uint32_t value(const State &s) {
	uint16x8_t tc = vdupq_n_u16(0), yc = vdupq_n_u16(0);
	for (int i = 0; i < ROWS; i += 2) {
		tc = vpadalq_u8(tc, vcntq_u8(vld1q_u8((const uint8_t *) (s.t + i))));
		yc = vpadalq_u8(yc, vcntq_u8(vld1q_u8((const uint8_t *) (s.y + i))));
	}
	return uint32_t(vaddvq_u16(tc)) * uint32_t(vaddvq_u16(yc));
}

inline uint32_t slot_of(uint64_t h, int bits) {
	return uint32_t((h * 0xD6E8FEB86659FD93ULL) >> (64 - bits));
}

}

void day18(input_t input) {
	size_t cap = 1024;
	std::unique_ptr<State[]> hist(new State[cap]);
	std::unique_ptr<uint64_t[]> hashes(new uint64_t[cap]);
	int hbits = 12;
	std::vector<uint32_t> table(1u << hbits, 0);

	parse(input.s, hist[0]);
	hashes[0] = hash_state(hist[0]);
	table[slot_of(hashes[0], hbits)] = 1;

	uint32_t base = 0, loop = 1, t = 0;
	for (;;) {
		if (t + 1 >= cap) {
			std::unique_ptr<State[]> nh(new State[cap * 2]);
			std::unique_ptr<uint64_t[]> nhs(new uint64_t[cap * 2]);
			memcpy(nh.get(), hist.get(), cap * sizeof(State));
			memcpy(nhs.get(), hashes.get(), cap * sizeof(uint64_t));
			hist.swap(nh);
			hashes.swap(nhs);
			cap *= 2;
		}
		if (2 * (t + 2) > table.size()) {
			hbits++;
			std::vector<uint32_t> nt(1u << hbits, 0);
			uint32_t m = (1u << hbits) - 1;
			for (uint32_t g = 0; g <= t; g++) {
				uint32_t sl = slot_of(hashes[g], hbits);
				while (nt[sl]) sl = (sl + 1) & m;
				nt[sl] = g + 1;
			}
			table.swap(nt);
		}
		uint64_t h = step(hist[t], hist[t + 1]);
		t++;
		hashes[t] = h;
		uint32_t m = uint32_t(table.size() - 1);
		uint32_t slot = slot_of(h, hbits);
		bool found = false;
		for (;;) {
			uint32_t e = table[slot];
			if (!e) {
				table[slot] = t + 1;
				break;
			}
			e--;
			if (hashes[e] == h && same(hist[e], hist[t])) {
				base = e;
				loop = t - e;
				found = true;
				break;
			}
			slot = (slot + 1) & m;
		}
		if (found) break;
	}

	auto reduce = [&](uint32_t g) { return g <= t ? g : base + (g - base) % loop; };
	uint32_t idx = base + (1000000000 - base) % loop;

	printf("Day 18 Part 1: %u\nDay 18 Part 2: %u\n", value(hist[reduce(10)]), value(hist[idx]));
}
