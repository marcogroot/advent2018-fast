#include "advent2018.h"
#include <arm_neon.h>

namespace {

constexpr uint8_t BASE[23] = {
	3,7,1,0,1,0,1,2,4,5,1,5,8,9,1,6,7,7,9,2,5,1,0 };

constexpr uint32_t PREMIX[9] = {
	0xfff94113, 0xffff0657, 0xfff94111, 0xfff94110, 0xffff9411,
	0xffff9410, 0xfffff941, 0xffff1812, 0xffffff94 };

constexpr size_t INPUT_LEN = 6;

constexpr size_t MIXERS_MEM  =  8 << 20;
constexpr size_t RECIPES_MEM = 22 << 20;
constexpr size_t GENERATE_LIMIT = RECIPES_MEM - (64 * 256);

constexpr size_t WIN_USE = 1 << 16;
constexpr size_t WIN_CAP = 1 << 19;
constexpr size_t CHUNK = 4096;
constexpr int PK = 12;
constexpr int PU = 2;

struct ExpandTables {
	uint8_t lo[256][16];
	uint8_t hi[256][16];
	uint8_t len[256];
};

constexpr ExpandTables make_tables() {
	ExpandTables t{};
	for (int m = 0; m < 256; m++) {
		int j = 0;
		for (int i = 0; i < 8; i++) {
			if (m & (1 << i)) {
				t.lo[m][j] = 0x80;
				t.hi[m][j] = 0x80;
				j++;
			}
			t.lo[m][j] = i;
			t.hi[m][j] = i + 8;
			j++;
		}
		t.len[m] = j;
		for (; j < 16; j++) {
			t.lo[m][j] = 0x80;
			t.hi[m][j] = 0x80;
		}
	}
	return t;
}

constexpr ExpandTables TAB = make_tables();

template <int K, int U>
size_t propagate(const uint8_t *r, size_t p, size_t end, uint8_t *&out, uint8_t *tmp) {
	size_t len = end - p;
	if (len < 128 * K) {
		uint8_t *o = out;
		do {
			uint8_t d = r[p];
			*o++ = d;
			p += d + 1;
		} while (p < end);
		out = o;
		return p;
	}
	const uint8_t *b[K + 1];
	for (int k = 0; k <= K; k++) {
		b[k] = r + p + len * k / K;
	}
	const uint8_t *q[K];
	uint8_t *o[K];
	uint8_t *src[K];
	for (int k = 0; k < K; k++) {
		q[k] = b[k];
		o[k] = k ? tmp + (b[k] - b[0]) + 16 * k : out;
		src[k] = o[k];
	}
	intptr_t lim[K];
	for (int k = 0; k < K; k++) {
		lim[k] = intptr_t(b[k + 1]) - 10 * (U - 1);
	}
	for (;;) {
		intptr_t t = -1;
		for (int k = 0; k < K; k++) {
			t &= intptr_t(q[k]) - lim[k];
		}
		if (t >= 0) {
			break;
		}
		for (int k = 0; k < K; k++) {
			const uint8_t *x = q[k];
			uint64_t w = 0;
			for (int u = 0; u < U; u++) {
				uint64_t d = x[u];
				w |= d << (8 * u);
				x += d;
			}
			memcpy(o[k], &w, U);
			q[k] = x + U;
			o[k] += U;
		}
	}
	for (;;) {
		bool any = false;
		for (int k = 0; k < K; k++) {
			const uint8_t *x = q[k];
			bool act = x < b[k + 1];
			any |= act;
			uint8_t d = *x;
			*o[k] = d;
			q[k] = act ? x + d + 1 : x;
			o[k] += act;
		}
		if (!any) {
			break;
		}
	}
	uint8_t *dst = o[0];
	const uint8_t *c = q[0];
	for (int k = 1; k < K; k++) {
		const uint8_t *g = b[k];
		const uint8_t *e = b[k + 1];
		size_t j = 0;
		while (g != c && c < e) {
			bool ag = g < c;
			uint8_t dg = *g, dc = *c;
			*dst = dc;
			dst += !ag;
			c = ag ? c : c + dc + 1;
			g = ag ? g + dg + 1 : g;
			j += ag;
		}
		if (g == c && c < e) {
			size_t n = o[k] - src[k] - j;
			memcpy(dst, src[k] + j, n);
			dst += n;
			c = q[k];
		}
	}
	out = dst;
	return c - r;
}

}

void day14(input_t in) {
	int input_n = 0;
	uint8_t input[INPUT_LEN];
	for (size_t i = 0; i < INPUT_LEN; i++) {
		input[i] = in.s[i] - '0';
		input_n = 10 * input_n + input[i];
	}

	auto mixers = reinterpret_cast<uint8_t *>(malloc(MIXERS_MEM));
	size_t mixers_size = 0;

	alignas(64) uint8_t win[WIN_CAP];
	std::copy(std::begin(BASE), std::end(BASE), win);
	size_t base = 0;
	size_t recipes_size = 23;

	char part1[11] = {0};
	bool part1_ready = false;
	const auto try_part1 = [&]() {
		if (!part1_ready && size_t(input_n) + 10 <= recipes_size) {
			for (int i = 0; i < 10; i++) {
				part1[i] = '0' + win[input_n + i - base];
			}
			part1_ready = true;
		}
	};

	int32_t premix[] = { int32_t(PREMIX[0]), int32_t(PREMIX[8]) };
	size_t next_idx = 23;
	alignas(64) uint8_t tmpbuf[WIN_CAP];
	size_t idx[2] = { 0, 0 };

	size_t search_offset = 0;
	long found_offset = -1;
	bool found = false;

	const auto append_recipe = [&](uint8_t recipe) {
		if (recipe >= 10) {
			recipe -= 10;
			if (recipes_size == next_idx) {
				mixers[mixers_size++] = 1;
				next_idx += 2;
			}
			win[recipes_size - base] = 1;
			recipes_size++;
		}
		if (recipes_size == next_idx) {
			mixers[mixers_size++] = recipe;
			next_idx += recipe + 1;
		}
		win[recipes_size - base] = recipe;
		recipes_size++;
		for (int i = 0; i < 2; i++) {
			if (idx[i] == mixers_size) {
				premix[i] = PREMIX[next_idx - recipes_size];
				idx[i] = 0;
			}
		}
	};

	const uint8x16_t n0 = vdupq_n_u8(input[0]);
	const uint8x16_t n1 = vdupq_n_u8(input[1]);
	const uint8x16_t n2 = vdupq_n_u8(input[2]);
	const uint8x16_t n3 = vdupq_n_u8(input[3]);
	const uint8x16_t n4 = vdupq_n_u8(input[4]);
	const uint8x16_t n5 = vdupq_n_u8(input[5]);

	const auto exact16 = [&](const uint8_t *p) -> int {
		uint8x16_t acc = vabdq_u8(vld1q_u8(p), n0);
		acc = vabaq_u8(acc, vld1q_u8(p + 1), n1);
		acc = vabaq_u8(acc, vld1q_u8(p + 2), n2);
		acc = vabaq_u8(acc, vld1q_u8(p + 3), n3);
		acc = vabaq_u8(acc, vld1q_u8(p + 4), n4);
		acc = vabaq_u8(acc, vld1q_u8(p + 5), n5);
		uint8x16_t eq = vceqzq_u8(acc);
		uint64_t m = vget_lane_u64(vreinterpret_u64_u8(
				vshrn_n_u16(vreinterpretq_u16_u8(eq), 4)), 0);
		return m ? int(__builtin_ctzll(m) >> 2) : -1;
	};

	const auto filter16 = [&](const uint8_t *p) {
		uint8x16_t acc = vabdq_u8(vld1q_u8(p), n0);
		acc = vabaq_u8(acc, vld1q_u8(p + 1), n1);
		return vabaq_u8(acc, vld1q_u8(p + 2), n2);
	};

	const auto search = [&]() {
		for (; search_offset + 96 <= recipes_size; search_offset += 64) {
			const uint8_t *p = win + (search_offset - base);
			uint8x16_t a = vminq_u8(filter16(p), filter16(p + 16));
			uint8x16_t b = vminq_u8(filter16(p + 32), filter16(p + 48));
			if (__builtin_expect(vminvq_u8(vminq_u8(a, b)) == 0, 0)) {
				for (int k = 0; k < 64; k += 16) {
					int m = exact16(p + k);
					if (m >= 0) {
						found_offset = search_offset + k + m;
						found = true;
						return;
					}
				}
			}
		}
	};

	const uint8x16_t nine = vdupq_n_u8(9);
	const uint8x16_t ten = vdupq_n_u8(10);
	const uint8x16_t ones = vdupq_n_u8(1);
	const uint8x16_t bitw = { 1,2,4,8,16,32,64,128, 1,2,4,8,16,32,64,128 };

	for (;;) {
		do {
			uint8_t recipe = 0;
			for (int i = 0; i < 2; i++) {
				if (~premix[i]) {
					recipe += premix[i] & 0xf;
					premix[i] >>= 4;
				} else {
					recipe += mixers[idx[i]++];
				}
			}
			append_recipe(recipe);
		} while ((premix[0] & premix[1]) != -1);

		for (;;) {
			size_t iterations = (mixers_size - std::max(idx[0], idx[1])) / 16;
			if (iterations == 0) {
				break;
			}
			if (iterations > CHUNK) {
				iterations = CHUNK;
			}

			const uint8_t *m0 = mixers + idx[0];
			const uint8_t *m1 = mixers + idx[1];
			uint8_t *out = win + (recipes_size - base);
			uint8_t *out_start = out;
			size_t i = 0;
			for (; i + 2 <= iterations; i += 2) {
				uint8x16_t s0 = vaddq_u8(vld1q_u8(m0 + 16 * i), vld1q_u8(m1 + 16 * i));
				uint8x16_t s1 = vaddq_u8(vld1q_u8(m0 + 16 * i + 16), vld1q_u8(m1 + 16 * i + 16));
				uint8x16_t t0 = vcgtq_u8(s0, nine);
				uint8x16_t t1 = vcgtq_u8(s1, nine);
				s0 = vsubq_u8(s0, vandq_u8(t0, ten));
				s1 = vsubq_u8(s1, vandq_u8(t1, ten));
				uint8x16_t b = vpaddq_u8(vandq_u8(t0, bitw), vandq_u8(t1, bitw));
				b = vpaddq_u8(b, b);
				b = vpaddq_u8(b, b);
				uint32_t mm = vgetq_lane_u32(vreinterpretq_u32_u8(b), 0);
				uint32_t ma = mm & 0xff, mb = (mm >> 8) & 0xff, mc = (mm >> 16) & 0xff, md = mm >> 24;
				uint8x16_t oa = vqtbx1q_u8(ones, s0, vld1q_u8(TAB.lo[ma]));
				uint8x16_t ob = vqtbx1q_u8(ones, s0, vld1q_u8(TAB.hi[mb]));
				uint8x16_t oc = vqtbx1q_u8(ones, s1, vld1q_u8(TAB.lo[mc]));
				uint8x16_t od = vqtbx1q_u8(ones, s1, vld1q_u8(TAB.hi[md]));
				uint32_t cn = vget_lane_u32(vreinterpret_u32_u8(vcnt_u8(vget_low_u8(b))), 0);
				uint32_t pre = (cn + 0x08080808u) * 0x01010101u;
				vst1q_u8(out, oa);
				vst1q_u8(out + (pre & 0xff), ob);
				vst1q_u8(out + ((pre >> 8) & 0xff), oc);
				vst1q_u8(out + ((pre >> 16) & 0xff), od);
				out += pre >> 24;
			}
			for (; i < iterations; i++) {
				uint8x16_t s = vaddq_u8(vld1q_u8(m0 + 16 * i), vld1q_u8(m1 + 16 * i));
				uint8x16_t t = vcgtq_u8(s, nine);
				s = vsubq_u8(s, vandq_u8(t, ten));
				uint8x16_t b = vandq_u8(t, bitw);
				b = vpaddq_u8(b, b);
				b = vpaddq_u8(b, b);
				b = vpaddq_u8(b, b);
				uint32_t mm = vgetq_lane_u16(vreinterpretq_u16_u8(b), 0);
				uint32_t mlo = mm & 0xff, mhi = mm >> 8;
				uint8x16_t olo = vqtbx1q_u8(ones, s, vld1q_u8(TAB.lo[mlo]));
				uint8x16_t ohi = vqtbx1q_u8(ones, s, vld1q_u8(TAB.hi[mhi]));
				vst1q_u8(out, olo);
				out += TAB.len[mlo];
				vst1q_u8(out, ohi);
				out += TAB.len[mhi];
			}
			idx[0] += 16 * iterations;
			idx[1] += 16 * iterations;
			recipes_size += out - out_start;

			{
				uint8_t *mo = mixers + mixers_size;
				next_idx = base + propagate<PK, PU>(win, next_idx - base, recipes_size - base, mo, tmpbuf);
				mixers_size = mo - mixers;
			}

			if (!found) {
				search();
			}
			try_part1();
			if (found && part1_ready) {
				goto done;
			}
			if (recipes_size > GENERATE_LIMIT) {
				found_offset = -1;
				try_part1();
				goto done;
			}

			if (recipes_size - base > WIN_USE) {
				size_t nb = std::min(std::min(next_idx, search_offset), recipes_size - 16) & ~size_t(15);
				memmove(win, win + (nb - base), recipes_size - nb);
				base = nb;
			}
		}

		do {
			append_recipe(mixers[idx[0]++] + mixers[idx[1]++]);
		} while ((premix[0] & premix[1]) == -1);
	}

done:
	free(mixers);
	printf("Day 14 Part 1: %s\nDay 14 Part 2: %ld\n", part1, found_offset);
}
