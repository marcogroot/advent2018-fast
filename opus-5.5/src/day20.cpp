#include "advent2018.h"
#include <arm_neon.h>

namespace {

constexpr uint64_t kRestore = 0x8000;
constexpr uint32_t kGridCells = 16384;
constexpr uint64_t kDummyBit = kGridCells;

constexpr uint64_t make_op(int32_t delta, uint64_t f, int32_t dd, uint64_t dummy) {
	return (uint64_t)(uint16_t)delta | (f << 16) | ((uint64_t)(uint8_t)dd << 32) | (dummy << 48);
}

struct OpTable {
	uint64_t op[256];
	constexpr OpTable() : op() {
		for (int i = 0; i < 256; i++)
			op[i] = make_op(0, 0, 0, kDummyBit);
		op['N'] = make_op(128, 1, 0, 0);
		op['S'] = make_op(-128, 1, 0, 0);
		op['E'] = make_op(1, 1, 0, 0);
		op['W'] = make_op(-1, 1, 0, 0);
		op['('] = make_op(0, 0, 1, kDummyBit);
		op['|'] = make_op(0, kRestore, 0, 0);
		op[')'] = make_op(0, kRestore, -1, 0);
	}
};

constexpr OpTable kOps;

#define D20_STEP(K) \
	asm volatile( \
		"ldrb %w[t0], [%[s], #" #K "]\n\t" \
		"ldr %[op], [%[ops], %x[t0], lsl #3]\n\t" \
		"str %w[cell], [%[slot1], %[d], lsl #2]\n\t" \
		"ands %w[t0], %w[d], %w[op], asr #31\n\t" \
		"ldr %w[t0], [%[slot], %x[t0], lsl #2]\n\t" \
		"add %w[t1], %w[cell], %w[op], sxth\n\t" \
		"csel %w[cell], %w[t0], %w[t1], ne\n\t" \
		"sbfx %[t1], %[op], #32, #8\n\t" \
		"add %[d], %[d], %[t1]\n\t" \
		"and %w[t0], %w[cell], #0x3fff\n\t" \
		"orr %x[t1], %x[t0], %x[op], lsr #48\n\t" \
		"ldrh %w[t1], [%[grid], %x[t1], lsl #1]\n\t" \
		"sub %w[t2], %w[t1], %w[op], lsr #16\n\t" \
		"add %w[op], %w[dist], %w[op], lsr #16\n\t" \
		"cmp %w[t2], %w[dist]\n\t" \
		"csel %w[dist], %w[t1], %w[op], lt\n\t" \
		"strh %w[dist], [%[grid], %x[t0], lsl #1]\n\t" \
		: [cell] "+r"(cell), [dist] "+r"(dist), [d] "+r"(d), [op] "=&r"(op), [t0] "=&r"(t0), [t1] "=&r"(t1), [t2] "=&r"(t2) \
		: [s] "r"(s), [ops] "r"(ops), [slot] "r"(slot), [slot1] "r"(slot1), [grid] "r"(grid) \
		: "cc", "memory")

}

void day20(input_t input) {
	alignas(64) uint16_t grid[2 * kGridCells];
	{
		uint16x8_t ff = vdupq_n_u16(0xFFFF);
		for (uint32_t i = 0; i < 2 * kGridCells; i += 32) {
			vst1q_u16(grid + i, ff);
			vst1q_u16(grid + i + 8, ff);
			vst1q_u16(grid + i + 16, ff);
			vst1q_u16(grid + i + 24, ff);
		}
	}

	size_t n = input.len;
	std::unique_ptr<uint32_t[]> slotbuf(new uint32_t[n / 2 + 4]);
	uint32_t *slot = slotbuf.get();
	uint32_t *slot1 = slot + 1;

	const unsigned char *s = (const unsigned char *)input.s;
	const unsigned char *e = s + n;
	uint32_t cell = 0x2040;
	uint32_t dist = 0;
	grid[cell] = 0;
	slot[0] = cell;
	uint64_t d = 0;
	const uint64_t *ops = kOps.op;
	uint64_t op, t0, t1, t2;

	for (; s + 4 <= e; s += 4) {
		D20_STEP(0);
		D20_STEP(1);
		D20_STEP(2);
		D20_STEP(3);
	}
	for (; s < e; s++) {
		D20_STEP(0);
	}

	uint16x8_t vmax = vdupq_n_u16(0);
	uint16x8_t vcnt = vdupq_n_u16(0);
	uint16x8_t thr = vdupq_n_u16(1000);
	uint16x8_t ff = vdupq_n_u16(0xFFFF);
	for (size_t j = 0; j < kGridCells; j += 8) {
		uint16x8_t v = vld1q_u16(grid + j);
		uint16x8_t invalid = vceqq_u16(v, ff);
		vmax = vmaxq_u16(vmax, vbicq_u16(v, invalid));
		vcnt = vsubq_u16(vcnt, vbicq_u16(vcgeq_u16(v, thr), invalid));
	}
	unsigned part1 = vmaxvq_u16(vmax);
	unsigned part2 = vaddlvq_u16(vcnt);

	printf("Day 20 Part 1: %u\nDay 20 Part 2: %u\n", part1, part2);
}
