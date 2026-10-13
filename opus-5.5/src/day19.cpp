#include "advent2018.h"
#include <arm_neon.h>

namespace {

enum : uint8_t { OP_BAD, OP_ADDI, OP_ADDR, OP_EQRR, OP_MULI, OP_MULR, OP_SETI, OP_SETR };

inline uint64_t load64(const uint8_t *p) {
	uint64_t v;
	memcpy(&v, p, 8);
	return v;
}

inline uint32_t load32(const uint8_t *p) {
	uint32_t v;
	memcpy(&v, p, 4);
	return v;
}

inline int8_t field(const uint8_t *p, unsigned len) {
	uint32_t w = (load32(p) ^ 0x30303030u) << (32 - 8 * len);
	return (int8_t)((w * 0x00640a01u) >> 24);
}

inline uint64_t newline_mask(const uint8_t *p) {
	const uint8x16_t nl = vdupq_n_u8('\n');
	const uint8x16_t bits = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
	uint8x16_t m0 = vandq_u8(vceqq_u8(vld1q_u8(p +  0), nl), bits);
	uint8x16_t m1 = vandq_u8(vceqq_u8(vld1q_u8(p + 16), nl), bits);
	uint8x16_t m2 = vandq_u8(vceqq_u8(vld1q_u8(p + 32), nl), bits);
	uint8x16_t m3 = vandq_u8(vceqq_u8(vld1q_u8(p + 48), nl), bits);
	uint8x16_t s = vpaddq_u8(vpaddq_u8(m0, m1), vpaddq_u8(m2, m3));
	s = vpaddq_u8(s, s);
	return vgetq_lane_u64(vreinterpretq_u64_u8(s), 0);
}

struct Ins {
	uint8_t op;
	int8_t a, b, c;
};

struct OpTable {
	uint8_t v[256];
	constexpr OpTable() : v() {
		v[(uint8_t)-7] = OP_ADDI;
		v[2] = OP_ADDR;
		v[66] = OP_EQRR;
		v[(uint8_t)-51] = OP_MULI;
		v[(uint8_t)-42] = OP_MULR;
		v[77] = OP_SETI;
		v[86] = OP_SETR;
	}
};

constexpr OpTable op_table;

inline uint8_t classify(int8_t h) {
	return op_table.v[(uint8_t)h];
}

template <typename Push>
inline bool parse_line(const uint8_t *p, const uint8_t *e, Push &push) {
	uint32_t w = load32(p);
	uint32_t lt = (w + 0x1f1f1f1fu) & ~(w + 0x05050505u) & ~w & 0x80808080u;
	const uint8_t *q = p + 5;
	uint64_t x = load64(q) ^ 0x3030303030303030ull;
	uint64_t nd = (((x & 0x7f7f7f7f7f7f7f7full) + 0x7676767676767676ull) | x) & 0x8080808080808080ull;
	unsigned s1 = __builtin_ctzll(nd | (1ull << 63)) >> 3;
	nd &= nd - 1;
	unsigned s2 = __builtin_ctzll(nd | (1ull << 63)) >> 3;
	const uint8_t *cs = q + s2 + 1;
	unsigned la = s1, lb = s2 - s1 - 1;
	size_t lc = e - cs;

	uint32_t xc = load32(cs) ^ 0x30303030u;
	uint32_t ndc = (((xc & 0x7f7f7f7fu) + 0x76767676u) | xc) & 0x80808080u;
	unsigned lcd = __builtin_ctz(ndc | 0x80000000u) >> 3;

	if ((lt != 0x80808080u) | (p[4] != ' ') |
	    (la - 1 > 2) | (lb - 1 > 2) | (lc - 1 > 2) | (lcd != lc) | (q[s1] != ' ') | (q[s2] != ' ')) {
		return false;
	}

	uint8_t h = (((w & 0xff) * 10 + ((w >> 8) & 0xff)) * 10 + ((w >> 16) & 0xff)) * 10 + (w >> 24) - 53328;
	push(classify(h), field(q, la), field(q + s1 + 1, lb), field(cs, lc));
	return true;
}

uint32_t divisor_sum(uint32_t n) {
	uint32_t sum = 1;

	auto try_factor = [&](uint32_t f) {
		if (n % f == 0) {
			uint32_t mult = 1, fk = 1;
			do {
				n /= f;
				fk *= f;
				mult += fk;
			} while (n % f == 0);
			sum *= mult;
		}
	};

	for (uint32_t f : { 2, 3, 5 }) {
		try_factor(f);
	}

	for (uint32_t f = 7, step = 0x62642424; f * f <= n; ) {
		try_factor(f);
		f += step & 15;
		step = (step << 28) | (step >> 4);
	}

	if (n > 1) {
		sum *= n + 1;
	}

	return sum;
}

}

void day19(input_t input) {
	const uint8_t *s = (const uint8_t *)input.s;
	const uint8_t *end = s + input.len;

	int8_t ipreg = -1;
	while (s < end) {
		uint8_t c = *s++ - '0';
		if (c < 6) {
			ipreg = c;
			break;
		}
	}
	if (ipreg == -1) {
		abort();
	}

	Ins small[64];
	std::vector<Ins> big;
	Ins *V = small;
	size_t cap = 64, size = 0;

	auto push = [&](uint8_t op, int8_t a, int8_t b, int8_t c) __attribute__((always_inline)) {
		if (size == cap) {
			if (V == small) {
				big.assign(small, small + size);
			}
			cap *= 2;
			big.resize(cap);
			V = big.data();
		}
		V[size++] = Ins{ op, a, b, c };
	};

	if (s < end && *s == '\n') {
		const uint8_t *p = s + 1;
		for (const uint8_t *blk = p; blk < end; blk += 64) {
			uint64_t m = newline_mask(blk);
			while (m) {
				const uint8_t *e = blk + __builtin_ctzll(m);
				m &= m - 1;
				if (e >= end || !parse_line(p, e, push)) {
					goto slow;
				}
				p = e + 1;
			}
		}
slow:
		s = p;
	}

	int8_t tmp[4];
	int n = 0, have = 0, i = 0;
	for (; s < end; s++) {
		uint8_t c = *s - '0';
		if (c < 100) {
			have = 1;
			n = 10 * n + c;
		} else if (have) {
			tmp[i] = n;
			n = have = 0;
			if (++i == 4) {
				i = 0;
				push(classify(tmp[0]), tmp[1], tmp[2], tmp[3]);
			}
		}
	}

	uint32_t part[2] = { 0, 1 };
	for (auto &P : part) {
		uint32_t R[8] = { P };
		uint32_t ip = R[ipreg];
		while (ip < size) {
			const Ins O = V[ip];
			R[ipreg] = ip;
			uint32_t Ra = R[O.a & 7], Rb = R[O.b & 7];
			uint32_t v;
			switch (O.op) {
			    case OP_ADDI: v = Ra + (uint32_t)O.b; break;
			    case OP_ADDR: v = Ra + Rb;            break;
			    case OP_EQRR: P = Rb;                 goto done;
			    case OP_MULI: v = Ra * (uint32_t)O.b; break;
			    case OP_MULR: v = Ra * Rb;            break;
			    case OP_SETI: v = (uint32_t)O.a;      break;
			    case OP_SETR: v = Ra;                 break;
			    default: abort();
			}
			if ((O.c & 7) == ipreg) {
				ip = v;
			} else {
				R[O.c & 7] = v;
			}
			ip++;
		}
done:
		P = divisor_sum(P);
	}

	printf("Day 19 Part 1: %u\nDay 19 Part 2: %u\n", part[0], part[1]);
}
