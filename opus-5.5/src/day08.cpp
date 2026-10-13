#include "advent2018.h"
#include <arm_neon.h>
#include <memory>
#include <alloca.h>

namespace {

struct ShufTable {
	uint64_t t[256];
	uint8_t c[256];
	constexpr ShufTable() : t(), c() {
		for (int m = 0; m < 256; m++) {
			uint64_t v = 0;
			int k = 0;
			for (int b = 0; b < 8; b++) {
				if (m & (1 << b)) v |= uint64_t(b) << (8 * k++);
			}
			for (; k < 8; k++) v |= uint64_t(0x80) << (8 * k);
			t[m] = v;
			c[m] = 0;
			for (int b = 0; b < 8; b++) c[m] += (m >> b) & 1;
		}
	}
};

constexpr ShufTable kShuf;

struct Frame {
	uint32_t remaining;
	uint32_t n_meta;
	uint32_t n_child;
	uint32_t base;
};

inline uint32_t sum_bytes(const uint8_t *p, uint32_t n) {
	static const uint8_t iota_arr[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
	uint8x16_t iota = vld1q_u8(iota_arr);
	uint32_t s = 0;
	while (n > 16) {
		s += vaddlvq_u8(vld1q_u8(p));
		p += 16;
		n -= 16;
	}
	uint8x16_t m = vcltq_u8(iota, vdupq_n_u8((uint8_t)n));
	s += vaddlvq_u8(vandq_u8(vld1q_u8(p), m));
	return s;
}

}

void day08(input_t input) {
	const uint8_t *s = (const uint8_t *)input.s;
	size_t len = input.len;

	std::unique_ptr<uint8_t[]> buf;
	uint8_t *out = (uint8_t *)alloca(len <= 262144 ? len / 2 + 64 : 16);
	if (len > 262144) {
		buf.reset(new uint8_t[len / 2 + 64]);
		out = buf.get();
	}
	uint8_t *o = out;

	const uint8x16_t zero = vdupq_n_u8('0');
	const uint8x16_t ten = vdupq_n_u8(10);
	static const uint8_t bits_arr[8] = {1,2,4,8,16,32,64,128};
	const uint8x8_t bits = vld1_u8(bits_arr);

	uint8x16_t prev_d = vdupq_n_u8(0);
	uint8x16_t prev_isd = vdupq_n_u8(0);
	uint8x16_t d = vsubq_u8(vld1q_u8(s), zero);
	uint8x16_t isd = vcltq_u8(d, ten);

	for (size_t i = 0; i < len; i += 16) {
		uint8x16_t nd = vsubq_u8(vld1q_u8(s + i + 16), zero);
		uint8x16_t nisd = vcltq_u8(nd, ten);

		uint8x16_t pd = vextq_u8(prev_d, d, 15);
		uint8x16_t pisd = vextq_u8(prev_isd, isd, 15);
		uint8x16_t next_isd = vextq_u8(isd, nisd, 1);

		uint8x16_t end = vbicq_u8(isd, next_isd);
		uint8x16_t val = vaddq_u8(d, vandq_u8(pisd, vmulq_u8(pd, ten)));

		unsigned lo = vaddv_u8(vand_u8(vget_low_u8(end), bits));
		unsigned hi = vaddv_u8(vand_u8(vget_high_u8(end), bits));

		uint8x8_t vlo = vtbl1_u8(vget_low_u8(val), vcreate_u8(kShuf.t[lo]));
		uint8x8_t vhi = vtbl1_u8(vget_high_u8(val), vcreate_u8(kShuf.t[hi]));
		vst1_u8(o, vlo);
		o += kShuf.c[lo];
		vst1_u8(o, vhi);
		o += kShuf.c[hi];

		prev_d = d;
		prev_isd = isd;
		d = nd;
		isd = nisd;
	}
	size_t count = o - out;
	memset(o, 0, 32);

	size_t heap_need = (count / 2 + 2) * sizeof(Frame) + (count / 2 + 32) * sizeof(uint32_t);
	std::unique_ptr<uint8_t[]> heap;
	uint8_t *mem = (uint8_t *)alloca(heap_need <= 262144 ? heap_need : 16);
	if (heap_need > 262144) {
		heap.reset(new uint8_t[heap_need]);
		mem = heap.get();
	}
	Frame *sp = (Frame *)mem;
	uint32_t *res = (uint32_t *)(mem + (count / 2 + 2) * sizeof(Frame));
	Frame *sbase = sp;

	static const uint8_t iota_arr[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
	const uint8x16_t iota = vld1q_u8(iota_arr);
	const uint8_t *p = out;
	uint32_t rsz = 0;
	int part1 = 0;
	uint32_t part2 = 0;

	for (;;) {
		uint32_t nc = p[0], nm = p[1];
		p += 2;
		if (nc) {
			*sp++ = Frame{nc, nm, nc, rsz};
			continue;
		}
		uint32_t val = sum_bytes(p, nm);
		p += nm;
		part1 += val;
		for (;;) {
			if (sp == sbase) {
				part2 = val;
				goto done;
			}
			res[rsz++] = val;
			Frame &f = sp[-1];
			if (--f.remaining) break;
			--sp;
			uint32_t base = f.base;
			uint32_t n = f.n_child;
			uint32_t m = f.n_meta;
			res[base + n] = 0;
			const uint32_t *r = res + base;
			part1 += sum_bytes(p, m);
			val = 0;
			if (m <= 16 && n < 256) {
				uint8x16_t msk = vcltq_u8(iota, vdupq_n_u8((uint8_t)m));
				uint8x16_t kv = vsubq_u8(vandq_u8(vld1q_u8(p), msk), vdupq_n_u8(1));
				kv = vminq_u8(kv, vdupq_n_u8((uint8_t)n));
				alignas(16) uint8_t ks[16];
				vst1q_u8(ks, kv);
				uint32_t v0 = 0, v1 = 0, v2 = 0, v3 = 0;
				for (int j = 0; j < 16; j += 4) {
					v0 += r[ks[j]];
					v1 += r[ks[j + 1]];
					v2 += r[ks[j + 2]];
					v3 += r[ks[j + 3]];
				}
				val = (v0 + v1) + (v2 + v3);
			} else {
				for (uint32_t j = 0; j < m; j++) {
					uint32_t k = (uint32_t)p[j] - 1;
					k = k < n ? k : n;
					val += r[k];
				}
			}
			p += m;
			rsz = base;
		}
	}
done:

	printf("Day 08 Part 1: %d\nDay 08 Part 2: %d\n", part1, part2);
}
