#include "advent2018.h"
#include <arm_neon.h>
#include <cstring>

namespace {

using Quad = std::array<int8_t, 4>;

void day16_generic(input_t input) {
	std::vector<Quad> V;
	Quad tmp;
	size_t prog_idx = -1;
	for (int8_t i = 0, n = 0, have = 0; input.len--; input.s++) {
		if (*input.s == ']') {
			prog_idx = V.size();
		}
		uint8_t c = *input.s - '0';
		if (c < 10) {
			have = 1;
			n = 10 * n + c;
		} else if (have) {
			have = 0;
			tmp[i] = n;
			n = 0;
			if (++i == 4) {
				i = 0;
				V.push_back(tmp);
			}
		}
	}
	prog_idx++;

	std::array<uint16_t, 16> M;
	M.fill(0xffff);

	int part1 = 0;
	for (auto ti = V.begin(), end = ti + prog_idx - 2; ti < end; ) {
		auto &R = *ti++;
		auto &O = *ti++;
		auto &A = *ti++;

		O[0] &= 15; O[1] &= 3; O[2] &= 3; O[3] &= 3;

		auto Ia = O[1],  Ib = O[2];
		auto Ra = R[Ia], Rb = R[Ib];

		uint16_t ok = 0;
		auto E = A[O[3]];
		ok |= (E == (Ra +  Rb)) <<  0;
		ok |= (E == (Ra +  Ib)) <<  1;
		ok |= (E == (Ra *  Rb)) <<  2;
		ok |= (E == (Ra *  Ib)) <<  3;
		ok |= (E == (Ra &  Rb)) <<  4;
		ok |= (E == (Ra &  Ib)) <<  5;
		ok |= (E == (Ra |  Rb)) <<  6;
		ok |= (E == (Ra |  Ib)) <<  7;
		ok |= (E == (Ra      )) <<  8;
		ok |= (E == (Ia      )) <<  9;
		ok |= (E == (Ia >  Rb)) << 10;
		ok |= (E == (Ra >  Ib)) << 11;
		ok |= (E == (Ra >  Rb)) << 12;
		ok |= (E == (Ia == Rb)) << 13;
		ok |= (E == (Ra == Ib)) << 14;
		ok |= (E == (Ra == Rb)) << 15;
		part1 += __builtin_popcount(ok) >= 3;
		M[O[0]] &= ok;
	}

	for (uint32_t ops = 0xffff, todo = 0xffff; todo; ) {
		for (uint32_t i = todo; i; i &= i - 1) {
			auto &m = M[__builtin_ctz(i)];
			m &= ops;
			if (!(m & (m - 1))) {
				ops ^= m;
				todo ^= i & -i;
			}
		}
	}

	std::array<uint16_t, 4> R = { 0, 0, 0, 0 };
	for (auto ip = V.begin() + prog_idx; ip != V.end(); ip++) {
		auto &O = *ip;
		O[0] &= 15; O[1] &= 3; O[2] &= 3; O[3] &= 3;
		auto instr = __builtin_ctz(M[O[0]]);
		auto Ia = O[1],  Ib = O[2],   Ic = O[3];
		auto Ra = R[Ia], Rb = R[Ib], &Rc = R[Ic];
		switch (instr) {
		    case  0: Rc = Ra +  Rb; break;
		    case  1: Rc = Ra +  Ib; break;
		    case  2: Rc = Ra *  Rb; break;
		    case  3: Rc = Ra *  Ib; break;
		    case  4: Rc = Ra &  Rb; break;
		    case  5: Rc = Ra &  Ib; break;
		    case  6: Rc = Ra |  Rb; break;
		    case  7: Rc = Ra |  Ib; break;
		    case  8: Rc = Ra      ; break;
		    case  9: Rc = Ia      ; break;
		    case 10: Rc = Ia >  Rb; break;
		    case 11: Rc = Ra >  Ib; break;
		    case 12: Rc = Ra >  Rb; break;
		    case 13: Rc = Ia == Rb; break;
		    case 14: Rc = Ra == Ib; break;
		    case 15: Rc = Ra == Rb; break;
		}
	}

	printf("Day 16 Part 1: %d\nDay 16 Part 2: %d\n", part1, R[0]);
}


#define D16_OP0(A, B, a, b) ((A) + (B))
#define D16_OP1(A, B, a, b) ((A) + (b))
#define D16_OP2(A, B, a, b) ((A) * (B))
#define D16_OP3(A, B, a, b) ((A) * (b))
#define D16_OP4(A, B, a, b) ((A) & (B))
#define D16_OP5(A, B, a, b) ((A) & (b))
#define D16_OP6(A, B, a, b) ((A) | (B))
#define D16_OP7(A, B, a, b) ((A) | (b))
#define D16_OP8(A, B, a, b) (A)
#define D16_OP9(A, B, a, b) (uint32_t)(a)
#define D16_OP10(A, B, a, b) (uint32_t)((uint32_t)(a) > ((B) & 0xffff))
#define D16_OP11(A, B, a, b) (uint32_t)(((A) & 0xffff) > (uint32_t)(b))
#define D16_OP12(A, B, a, b) (uint32_t)(((A) & 0xffff) > ((B) & 0xffff))
#define D16_OP13(A, B, a, b) (uint32_t)((uint32_t)(a) == ((B) & 0xffff))
#define D16_OP14(A, B, a, b) (uint32_t)(((A) & 0xffff) == (uint32_t)(b))
#define D16_OP15(A, B, a, b) (uint32_t)(((A) & 0xffff) == ((B) & 0xffff))
#define D16_REG_(x) r##x
#define D16_REG(x) D16_REG_(x)
#define D16_CASE(op, a, b, c) case ((op) << 6 | (a) | (b) << 2 | (c) << 4): D16_REG(c) = D16_OP##op(D16_REG(a), D16_REG(b), a, b); break;
#define D16_C(op, a, b) D16_CASE(op, a, b, 0) D16_CASE(op, a, b, 1) D16_CASE(op, a, b, 2) D16_CASE(op, a, b, 3)
#define D16_B(op, a) D16_C(op, a, 0) D16_C(op, a, 1) D16_C(op, a, 2) D16_C(op, a, 3)
#define D16_A(op) D16_B(op, 0) D16_B(op, 1) D16_B(op, 2) D16_B(op, 3)
#define D16_ALL D16_A(0) D16_A(1) D16_A(2) D16_A(3) D16_A(4) D16_A(5) D16_A(6) D16_A(7) D16_A(8) D16_A(9) D16_A(10) D16_A(11) D16_A(12) D16_A(13) D16_A(14) D16_A(15) default: __builtin_unreachable();

constexpr int K = 4;
constexpr size_t SEG = 512;
constexpr size_t PSEG = 512;

inline uint64_t load64(const unsigned char *p) {
	uint64_t w;
	memcpy(&w, p, 8);
	return w;
}

constexpr uint64_t OPND_MASK = 0x00FFF0FFF0FFF0FFull;
constexpr uint64_t OPND_EXPECT = 0x000A302030203020ull;
constexpr uint64_t DIG_LO = 0x00000F000F000F00ull;
constexpr uint64_t DIG_ADD = 0x0000060006000600ull;
constexpr uint64_t DIG_BAD = 0x0000100010001000ull;

struct SampleOut {
	uint32_t *x;
	uint16_t *y;
};

__attribute__((always_inline)) inline const unsigned char *sample_step(const unsigned char *p, SampleOut &o, size_t &n, uint64_t &bad) {
	uint32_t d0 = p[21], d1 = p[22];
	uint32_t two = d1 != ' ';
	uint32_t op = (two ? d0 * 10 + d1 : d0) & 15;
	const unsigned char *r = p + 22 + two;
	uint64_t w = load64(r);
	uint32_t a = (w >> 8) & 3, b = (w >> 24) & 3, c = (w >> 40) & 3;
	const unsigned char *t = r + 7;
	uint16_t h0; memcpy(&h0, p + 19, 2);
	uint32_t h1; memcpy(&h1, r + 26, 4);
	bad |= ((w & OPND_MASK) ^ OPND_EXPECT) | (((w & DIG_LO) + DIG_ADD) & DIG_BAD);
	bad |= (p[0] ^ 'B') | (h0 ^ (']' | '\n' << 8)) | ((h1 & 0xffffff) ^ (']' | '\n' << 8 | '\n' << 16)) | (((d0 - '0') + 6) & ~15u) | (two & (((d1 - '0') + 6) & ~15u));
	o.x[n] = p[9 + 3 * a] | (uint32_t)p[9 + 3 * b] << 8 | a << 16 | b << 24;
	o.y[n] = t[9 + 3 * c] | op << 8;
	n++;
	return t + 22;
}

__attribute__((always_inline)) inline const unsigned char *prog_step(const unsigned char *q, uint8_t *ops, uint8_t *args, size_t &n, uint64_t &bad) {
	uint32_t d0 = q[0], d1 = q[1];
	uint32_t two = d1 != ' ';
	uint32_t op = (two ? d0 * 10 + d1 : d0) & 15;
	const unsigned char *r = q + 1 + two;
	uint64_t w = load64(r);
	bad |= ((w & OPND_MASK) ^ OPND_EXPECT) | (((w & DIG_LO) + DIG_ADD) & DIG_BAD) | (((d0 - '0') + 6) & ~15u) | (two & (((d1 - '0') + 6) & ~15u));
	ops[n] = op;
	args[n] = ((w >> 8) & 3) | ((w >> 22) & 0xc) | ((w >> 36) & 0x30);
	n++;
	return r + 7;
}

bool day16_fast(const char *start, const char *end) {
	const unsigned char *beg = (const unsigned char *)start;
	const unsigned char *fin = (const unsigned char *)end;
	if (fin - beg < 64 || beg[0] != 'B') return false;

	const unsigned char *lb = nullptr;
	{
		const unsigned char *p = fin;
		const uint8x16_t br = vdupq_n_u8(']');
		while (p - 16 >= beg) {
			p -= 16;
			uint8x16_t eq = vceqq_u8(vld1q_u8(p), br);
			uint64_t m = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(eq), 4)), 0);
			if (m) {
				lb = p + ((63 - __builtin_clzll(m)) >> 2);
				break;
			}
		}
		if (!lb) return false;
	}
	const unsigned char *send = lb + 3;
	if (send > fin) return false;

	alignas(16) uint32_t X[K * SEG];
	alignas(16) uint16_t Y[K * SEG];
	size_t cnt[K];
	uint64_t bad = 0;
	{
		const unsigned char *ss[K + 1];
		ss[0] = beg;
		ss[K] = send;
		size_t len = send - beg;
		if ((len / K + 52) / 51 + 2 > SEG - 16) return false;
		for (int k = 1; k < K; k++) {
			const unsigned char *p = beg + len * k / K;
			while (*p != 'B' && p < send) p++;
			ss[k] = p;
		}
		const unsigned char *s[K];
		SampleOut o[K];
		for (int k = 0; k < K; k++) {
			s[k] = ss[k];
			cnt[k] = 0;
			size_t base = k * SEG;
			o[k] = { X + base, Y + base };
		}
		while (s[0] < ss[1] && s[1] < ss[2] && s[2] < ss[3] && s[3] < ss[4]) {
			s[0] = sample_step(s[0], o[0], cnt[0], bad);
			s[1] = sample_step(s[1], o[1], cnt[1], bad);
			s[2] = sample_step(s[2], o[2], cnt[2], bad);
			s[3] = sample_step(s[3], o[3], cnt[3], bad);
		}
		for (int k = 0; k < K; k++) {
			while (s[k] < ss[k + 1]) s[k] = sample_step(s[k], o[k], cnt[k], bad);
			bad |= s[k] != ss[k + 1];
		}
	}
	if (bad) return false;

	alignas(16) uint8_t LO[K * SEG], HI[K * SEG];
	int part1 = 0;
	const uint8x16_t one = vdupq_n_u8(1);
	const uint8x16_t zc = vdupq_n_u8('0');
	uint8x16_t badv = vdupq_n_u8(0);
	for (int k = 0; k < K; k++) {
		size_t base = k * SEG, n = cnt[k];
		size_t nv = (n + 15) & ~(size_t)15;
		for (size_t i = n; i < nv; i++) {
			X[base + i] = '0' | '0' << 8;
			Y[base + i] = '9';
		}
		uint8x16_t acc = vdupq_n_u8(0);
		for (size_t i = base; i < base + nv; i += 16) {
			uint8x16x4_t xv = vld4q_u8((const uint8_t *)(X + i));
			uint8x16x2_t yv = vld2q_u8((const uint8_t *)(Y + i));
			uint8x16_t Ra = vsubq_u8(xv.val[0], zc), Rb = vsubq_u8(xv.val[1], zc);
			uint8x16_t Ia = xv.val[2], Ib = xv.val[3];
			uint8x16_t E = vsubq_u8(yv.val[0], zc);
			badv = vorrq_u8(badv, vcgtq_u8(vmaxq_u8(vmaxq_u8(Ra, Rb), E), vdupq_n_u8(9)));
			auto bit = [&](uint8x16_t v, int b) { return vandq_u8(vceqq_u8(E, v), vdupq_n_u8(1 << b)); };
			uint8x16_t lo = bit(vaddq_u8(Ra, Rb), 0);
			lo = vorrq_u8(lo, bit(vaddq_u8(Ra, Ib), 1));
			lo = vorrq_u8(lo, bit(vmulq_u8(Ra, Rb), 2));
			lo = vorrq_u8(lo, bit(vmulq_u8(Ra, Ib), 3));
			lo = vorrq_u8(lo, bit(vandq_u8(Ra, Rb), 4));
			lo = vorrq_u8(lo, bit(vandq_u8(Ra, Ib), 5));
			lo = vorrq_u8(lo, bit(vorrq_u8(Ra, Rb), 6));
			lo = vorrq_u8(lo, bit(vorrq_u8(Ra, Ib), 7));
			uint8x16_t hi = bit(Ra, 0);
			hi = vorrq_u8(hi, bit(Ia, 1));
			hi = vorrq_u8(hi, bit(vandq_u8(vcgtq_u8(Ia, Rb), one), 2));
			hi = vorrq_u8(hi, bit(vandq_u8(vcgtq_u8(Ra, Ib), one), 3));
			hi = vorrq_u8(hi, bit(vandq_u8(vcgtq_u8(Ra, Rb), one), 4));
			hi = vorrq_u8(hi, bit(vandq_u8(vceqq_u8(Ia, Rb), one), 5));
			hi = vorrq_u8(hi, bit(vandq_u8(vceqq_u8(Ra, Ib), one), 6));
			hi = vorrq_u8(hi, bit(vandq_u8(vceqq_u8(Ra, Rb), one), 7));
			vst1q_u8(LO + i, lo);
			vst1q_u8(HI + i, hi);
			uint8x16_t c = vaddq_u8(vcntq_u8(lo), vcntq_u8(hi));
			acc = vsubq_u8(acc, vcgtq_u8(c, vdupq_n_u8(2)));
			if (((i - base) & 0xff0) == 0xf0) { part1 += vaddlvq_u8(acc); acc = vdupq_n_u8(0); }
		}
		part1 += vaddlvq_u8(acc);
	}
	if (vmaxvq_u8(badv)) return false;

	uint16_t MM[4][16];
	for (int j = 0; j < 4; j++) for (int i = 0; i < 16; i++) MM[j][i] = 0xffff;
	for (int k = 0; k < K; k++) {
		size_t base = k * SEG, n = cnt[k], nv = (n + 3) & ~(size_t)3;
		for (size_t i = base + n; i < base + nv; i++) { Y[i] = 0; LO[i] = HI[i] = 0xff; }
		for (size_t i = base; i < base + nv; i += 4) {
			MM[0][Y[i + 0] >> 8] &= LO[i + 0] | HI[i + 0] << 8;
			MM[1][Y[i + 1] >> 8] &= LO[i + 1] | HI[i + 1] << 8;
			MM[2][Y[i + 2] >> 8] &= LO[i + 2] | HI[i + 2] << 8;
			MM[3][Y[i + 3] >> 8] &= LO[i + 3] | HI[i + 3] << 8;
		}
	}
	uint16_t M[16];
	for (int i = 0; i < 16; i++) M[i] = MM[0][i] & MM[1][i] & MM[2][i] & MM[3][i];
	for (uint32_t ops = 0xffff, todo = 0xffff; todo; ) {
		uint32_t before = todo;
		for (uint32_t i = todo; i; i &= i - 1) {
			uint32_t m = M[__builtin_ctz(i)] &= ops;
			if (!(m & (m - 1))) {
				ops ^= m;
				todo ^= i & -i;
			}
		}
		if (todo == before) return false;
	}
	uint8_t map[16];
	for (int i = 0; i < 16; i++) map[i] = __builtin_ctz(M[i]);

	const unsigned char *ps = send;
	while (ps < fin && *ps == '\n') ps++;

	const unsigned char *pend = fin;
	if (pend > ps && pend[-1] != '\n') {
		while (pend > ps && pend[-1] != '\n') pend--;
	}

	alignas(16) uint8_t POPS[K * PSEG], PARG[K * PSEG];
	size_t pcnt[K];
	{
		const unsigned char *ss[K + 1];
		ss[0] = ps;
		ss[K] = pend;
		size_t len = pend - ps;
		if ((len / K + 9) / 8 + 2 > PSEG) return false;
		for (int k = 1; k < K; k++) {
			const unsigned char *p = ps + len * k / K;
			if (p > ps) {
				while (p < pend && p[-1] != '\n') p++;
			}
			ss[k] = p;
		}
		const unsigned char *s[K];
		for (int k = 0; k < K; k++) { s[k] = ss[k]; pcnt[k] = 0; }
		while (s[0] < ss[1] && s[1] < ss[2] && s[2] < ss[3] && s[3] < ss[4]) {
			s[0] = prog_step(s[0], POPS + 0 * PSEG, PARG + 0 * PSEG, pcnt[0], bad);
			s[1] = prog_step(s[1], POPS + 1 * PSEG, PARG + 1 * PSEG, pcnt[1], bad);
			s[2] = prog_step(s[2], POPS + 2 * PSEG, PARG + 2 * PSEG, pcnt[2], bad);
			s[3] = prog_step(s[3], POPS + 3 * PSEG, PARG + 3 * PSEG, pcnt[3], bad);
		}
		for (int k = 0; k < K; k++) {
			while (s[k] < ss[k + 1]) s[k] = prog_step(s[k], POPS + k * PSEG, PARG + k * PSEG, pcnt[k], bad);
			bad |= s[k] != ss[k + 1];
		}
	}
	if (bad) return false;

	uint32_t r0 = 0, r1 = 0, r2 = 0, r3 = 0;
	for (int k = 0; k < K; k++) {
		const uint8_t *po = POPS + k * PSEG, *pa = PARG + k * PSEG;
		for (size_t i = 0, n = pcnt[k]; i < n; i++) {
			switch ((uint32_t)map[po[i]] << 6 | pa[i]) {
				D16_ALL
			}
		}
	}

	printf("Day 16 Part 1: %d\nDay 16 Part 2: %d\n", part1, (int)(r0 & 0xffff));
	return true;
}

}

void day16(input_t input) {
	if (!day16_fast(input.s, input.s + input.len)) day16_generic(input);
}
