#include "advent2018.h"
#include <arm_neon.h>

namespace {

struct Game {
	uint32_t *mem;
	uint32_t *rd;
	uint32_t *wr;
	uint32x4_t W;
	uint32_t b;
	size_t cur_player;
	size_t n_players;
	uint64_t *score;
	uint32_t *tq;
	uint32_t *tqend;
	size_t tp;
	uint32_t tturn;

	template <bool Late>
	void simulate(size_t count) {
		uint32_t scratch[40];
		uint32_t *tq = this->tq, *tqend = this->tqend;
		size_t tp = this->tp;
		uint32_t tturn = this->tturn;
		static const uint32_t k0a[4] = { 0, 1, (uint32_t) -3, 2 };
		static const uint32_t k1a[4] = { 0, 3, (uint32_t) -2, 4 };
		static const uint32_t k2a[4] = { 0, 5, (uint32_t) -1, 6 };
		static const uint32_t kma[4] = { 7, 8, 9, 10 };
		const uint32x4_t k0 = vld1q_u32(k0a), k1 = vld1q_u32(k1a), k2 = vld1q_u32(k2a);
		const uint32x4_t km = vld1q_u32(kma);
		const uint32x4_t four = vdupq_n_u32(4), step = vdupq_n_u32(23);
		uint32x4_t B = vdupq_n_u32(b);
		uint32x4_t W = this->W;
		uint32_t *rd = this->rd, *wr = this->wr;
		size_t cur = cur_player, np = n_players;
		uint64_t *sc = score;
		uint32_t turn = b + 23;
		for (; count--; ) {
			uint32x4_t V0 = vld1q_u32(rd);
			uint32x4_t V1 = vld1q_u32(rd + 4);
			uint32x4_t V2 = vld1q_u32(rd + 8);
			uint32x4_t V3 = vld1q_u32(rd + 12);
			rd += 16;
			uint32x4_t O0 = vcopyq_laneq_u32(vaddq_u32(B, k0), 0, W, 1);
			uint32x4_t O1 = vcopyq_laneq_u32(vaddq_u32(B, k1), 0, W, 2);
			uint32x4_t O2 = vcopyq_laneq_u32(vaddq_u32(B, k2), 0, W, 3);
			uint32x4_t Ma = vaddq_u32(B, km);
			uint32x4_t Mb = vaddq_u32(Ma, four);
			uint32x4_t Mc = vaddq_u32(Mb, four);
			uint32x4_t Md = vaddq_u32(Mc, four);
			uint32_t *dst = Late ? scratch : wr;
			uint32x4x4_t X0, X1;
			X0.val[0] = O0; X0.val[1] = O1; X0.val[2] = O2; X0.val[3] = vzip1q_u32(V0, Ma);
			X1.val[0] = vzip2q_u32(V0, Ma); X1.val[1] = vzip1q_u32(V1, Mb); X1.val[2] = vzip2q_u32(V1, Mb); X1.val[3] = vzip1q_u32(V2, Mc);
			vst1q_u32_x4(dst, X0);
			vst1q_u32_x4(dst + 16, X1);
			uint32x4x2_t X2; X2.val[0] = vzip2q_u32(V2, Mc); X2.val[1] = Md;
			vst1q_u32_x2(dst + 32, X2);
			if (Late) {
				while (tq < wr + 37 && tq < tqend) {
					sc[tp] += tturn + scratch[tq - wr];
					tq += 16;
					tturn += 23;
					tp++;
					tp &= -(size_t) (tp < np);
				}
			}
			wr += 37;
			cur++;
			cur &= -(size_t) (cur < np);
			sc[cur] += turn + vgetq_lane_u32(V3, 0);
			turn += 23;
			W = V3;
			B = vaddq_u32(B, step);
		}
		this->W = W;
		this->rd = rd;
		this->wr = wr;
		this->cur_player = cur;
		this->b = turn - 23;
		this->tq = tq;
		this->tp = tp;
		this->tturn = tturn;
	}
};


struct Late {
	const uint32_t *rd;
	uint32_t b;
	size_t cur, tp, np;
	uint32_t tturn;
	uint64_t *sc;

	inline void inc(size_t& p) {
		p++;
		p &= -(size_t) (p < np);
	}

	inline uint32_t value(int t) const {
		if ((t & 1) || t == 36) return b + 1 + t / 2;
		int i = t / 2;
		if (i >= 6) return rd[i - 6];
		if (i & 1) return b - 3 + i / 2;
		return rd[i / 2 - 3];
	}

	template <int O>
	inline void round() {
		inc(cur);
		sc[cur] += b + 23 + rd[12];
		sc[tp] += tturn + value(O);
		inc(tp);
		sc[tp] += tturn + 23 + value(O + 16);
		inc(tp);
		if (O + 32 <= 36) {
			sc[tp] += tturn + 46 + value(O + 32);
			inc(tp);
			tturn += 69;
		} else {
			tturn += 46;
		}
		rd += 16;
		b += 23;
	}

	static constexpr int next(int o) {
		return o + 32 <= 36 ? o + 48 - 37 : o + 32 - 37;
	}
};
}

void day09(input_t input) {
	uint32_t n_players = 0, n_points = 0;
	for (uint32_t n = 0; input.len--; input.s++) {
		uint8_t c = *input.s - '0';
		if (c < 10) {
			n = 10 * n + c;
		} else if (n && *input.s == ' ') {
			(n_players ? n_points : n_players) = n;
			n = 0;
		}
	}

	static const uint32_t initial[22] = {
		 2, 20, 10, 21,  5, 22, 11,  1, 12,  6, 13,
		 3, 14,  7, 15,  0, 16,  8, 17,  4, 18, 19,
	};

	if (n_players % 23 == 0) {
		n_players /= 23;
	}

	size_t n_rounds1 = n_points / 23;
	size_t total2 = (size_t) 100 * n_points / 23 - 1;
	size_t s1 = n_rounds1 - 1;
	size_t s2 = (16 * total2 + 17) / 37 + 1;
	if (s2 < s1) s2 = s1;
	if (s2 > total2) s2 = total2;

	size_t lim = 16 * s2;
	size_t sB = lim > 16 ? (lim - 16 + 36) / 37 : 0;
	if (sB < s1) sB = s1;
	if (sB > s2) sB = s2;
	size_t cap = std::max(lim, 16 + 37 * sB) + 64;
	uint32_t *mem = (uint32_t *) malloc(cap * sizeof(uint32_t));
	std::copy(initial + 6, initial + 22, mem);

	std::vector<uint64_t> score(n_players);
	score[0] = 32;

	Game g;
	g.mem = mem;
	g.rd = mem;
	g.wr = mem + 16;
	uint32_t winit[4] = { 0, 2, 10, 5 };
	g.W = vld1q_u32(winit);
	g.b = 23;
	g.cur_player = 0;
	g.n_players = n_players;
	g.score = score.data();
	g.tq = nullptr;
	g.tqend = nullptr;
	g.tp = 0;
	g.tturn = 0;

	g.simulate<false>(s1);
	uint64_t part1 = 0;
	for (auto s : score) part1 = std::max(part1, s);

	g.simulate<false>(sB - s1);

	size_t rem = total2 - s2;
	uint32_t *q = mem + lim + 12, *qend = q + 16 * rem;
	uint32_t *wB = g.wr;
	size_t tp = (s2 + 1) % n_players;
	uint32_t tturn = (uint32_t) (23 * (s2 + 1) + 23);
	uint64_t *sc = score.data();
	for (; q < qend && q < wB; q += 16, tturn += 23) {
		sc[tp] += tturn + *q;
		tp++;
		tp &= -(size_t) (tp < n_players);
	}
	Late L;
	L.rd = g.rd;
	L.b = g.b;
	L.cur = g.cur_player;
	L.tp = tp;
	L.np = n_players;
	L.tturn = tturn;
	L.sc = sc;
	size_t w = wB - mem;
	size_t qi = q - mem, qe = qend - mem;
	int o = (int) (qi - w);
	size_t nlate = s2 - sB;
	while (nlate && qi + 48 < qe) {
		nlate--;
		size_t h;
		switch (o) {
#define C(O) case O: L.round<O>(); o = Late::next(O); h = (O + 32 <= 36) ? 3 : 2; break;
		C(0) C(1) C(2) C(3) C(4) C(5) C(6) C(7) C(8) C(9) C(10) C(11) C(12) C(13) C(14) C(15)
#undef C
		default: __builtin_unreachable();
		}
		qi += 16 * h;
	}
	while (nlate--) {
		L.inc(L.cur);
		L.sc[L.cur] += L.b + 23 + L.rd[12];
		while (o <= 36 && qi < qe) {
			L.sc[L.tp] += L.tturn + L.value(o);
			L.inc(L.tp);
			L.tturn += 23;
			o += 16;
			qi += 16;
		}
		o -= 37;
		L.rd += 16;
		L.b += 23;
	}

	uint64_t part2 = 0;
	for (auto s : score) part2 = std::max(part2, s);

	printf("Day 09 Part 1: %lu\nDay 09 Part 2: %lu\n", part1, part2);

	free(mem);
}
