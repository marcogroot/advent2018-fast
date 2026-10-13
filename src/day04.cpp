#include "advent2018.h"
#include <arm_neon.h>

namespace {

constexpr int MAXDAY = 400;
constexpr int HSIZE = 256;
constexpr int MAXG = 256;

const uint16_t month_start[16] = {
	0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365, 365, 365 };

inline uint64_t prefix_xor(uint64_t x) {
	x ^= x << 1;
	x ^= x << 2;
	x ^= x << 4;
	x ^= x << 8;
	x ^= x << 16;
	x ^= x << 32;
	return x;
}

inline int first_index_of(const uint8x16_t t[4], uint8_t v) {
	uint8x16_t vv = vdupq_n_u8(v);
	for (int k = 0; k < 4; k++) {
		uint8x16_t eq = vceqq_u8(t[k], vv);
		uint64_t m = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(eq), 4)), 0);
		if (m) return k * 16 + (__builtin_ctzll(m) >> 2);
	}
	return -1;
}

}

void day04(input_t input) {
	uint64_t toggles[MAXDAY];
	uint8_t day_guard[MAXDAY];
	memset(toggles, 0, sizeof(toggles));
	memset(day_guard, 0xff, sizeof(day_guard));

	int32_t hkey[HSIZE];
	uint8_t hval[HSIZE];
	memset(hkey, 0xff, sizeof(hkey));
	int32_t gid[MAXG];
	uint16_t gcount[MAXG + 1];
	int ng = 0;

	const char *p = input.s, *end = input.s + input.len;
	while (p + 27 <= end) {
		unsigned month = (p[6] - '0') * 10 + (p[7] - '0');
		unsigned day = (p[9] - '0') * 10 + (p[10] - '0');
		unsigned d = month_start[month & 15] + day;
		char c = p[19];
		if (c == 'G') {
			const char *q = p + 26;
			int id = 0;
			while ((unsigned)(*q - '0') < 10) id = id * 10 + (*q++ - '0');
			d += (p[12] != '0') | (p[13] != '0');
			unsigned h = (uint32_t(id) * 2654435761u) >> 24;
			while (hkey[h] != id && hkey[h] != -1) h = (h + 1) & (HSIZE - 1);
			if (hkey[h] == -1) {
				hkey[h] = id;
				hval[h] = ng;
				gcount[ng] = 0;
				gid[ng++] = id;
			}
			day_guard[d] = hval[h];
			gcount[hval[h]]++;
			p = q + 14;
		} else {
			unsigned minute = (p[15] - '0') * 10 + (p[16] - '0');
			toggles[d] ^= uint64_t(1) << minute;
			if (c == 'f') { p += 32; asm volatile(""); } else { p += 28; }
		}
	}

	uint16_t gstart[MAXG + 1];
	unsigned acc = 0;
	for (int i = 0; i < ng; i++) {
		gstart[i] = acc;
		acc += gcount[i];
	}
	gstart[ng] = acc;
	uint16_t pos[MAXG];
	memcpy(pos, gstart, sizeof(uint16_t) * ng);
	uint64_t sorted[MAXDAY];
	for (int d = 0; d < MAXDAY; d++) {
		unsigned g = day_guard[d];
		if (g != 0xff) sorted[pos[g]++] = prefix_xor(toggles[d]);
	}

	static const uint8_t bitpat_b[16] = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
	static const uint8_t idx_b[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 };
	const uint8x16_t bitpat = vld1q_u8(bitpat_b);
	const uint8x16_t idx0 = vld1q_u8(idx_b);
	const uint8x16_t two = vdupq_n_u8(2);
	const uint8x16_t idx1 = vaddq_u8(idx0, two);
	const uint8x16_t idx2 = vaddq_u8(idx1, two);
	const uint8x16_t idx3 = vaddq_u8(idx2, two);

	int64_t best_sleep = -1;
	int part1 = 0;
	unsigned best_minute_count = 0;
	int part2 = 0;
	for (int g = 0; g < ng; g++) {
		uint8x16_t t0 = vdupq_n_u8(0), t1 = t0, t2 = t0, t3 = t0;
		int64_t total = 0;
		for (unsigned k = gstart[g]; k < gstart[g + 1]; k++) {
			uint64_t asleep = sorted[k];
			total += __builtin_popcountll(asleep);
			uint8x16_t m = vreinterpretq_u8_u64(vdupq_n_u64(asleep));
			t0 = vsubq_u8(t0, vtstq_u8(vqtbl1q_u8(m, idx0), bitpat));
			t1 = vsubq_u8(t1, vtstq_u8(vqtbl1q_u8(m, idx1), bitpat));
			t2 = vsubq_u8(t2, vtstq_u8(vqtbl1q_u8(m, idx2), bitpat));
			t3 = vsubq_u8(t3, vtstq_u8(vqtbl1q_u8(m, idx3), bitpat));
		}
		uint8x16_t t[4] = { t0, t1, t2, t3 };
		uint8_t mx = vmaxvq_u8(vmaxq_u8(vmaxq_u8(t0, t1), vmaxq_u8(t2, t3)));
		if (total > best_sleep) {
			best_sleep = total;
			part1 = gid[g] * first_index_of(t, mx);
		}
		if (mx > best_minute_count) {
			best_minute_count = mx;
			part2 = gid[g] * first_index_of(t, mx);
		}
	}

	printf("Day 04 Part 1: %d\nDay 04 Part 2: %d\n", part1, part2);
}
