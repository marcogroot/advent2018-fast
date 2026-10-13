#include "advent2018.h"

namespace {

constexpr int N_TYPES = 5;
constexpr int NG = 20;

struct State {
	uint32_t pw[NG];
	uint32_t un[NG];
};

struct Army {
	uint32_t hp[NG];
	uint32_t dm[NG];
	uint32_t wk[NG];
	uint8_t fac[NG];
	uint8_t type[NG];
	uint32_t weak[N_TYPES];
	uint32_t vuln[N_TYPES];
	uint32_t alive0[2];
	uint32_t memb[NG];
};

constexpr uint16_t tag(const char *str) {
	return uint16_t(uint8_t(str[0])) << 8 | uint8_t(str[1]);
}

constexpr int NM = 2 + 2 * N_TYPES;

bool harmless(const Army &A, const State &S, const uint32_t *alive, int X) {
	uint32_t mw[N_TYPES], mv[N_TYPES];
	for (int t = 0; t < N_TYPES; t++) mw[t] = mv[t] = ~0u;
	for (uint32_t m = alive[!X]; m; m &= m - 1) {
		int y = __builtin_ctz(m);
		uint32_t h = A.hp[y];
		for (int t = 0; t < N_TYPES; t++) {
			if ((A.wk[y] >> t) & 1) mw[t] = std::min(mw[t], h);
			if ((A.vuln[t] >> y) & 1) mv[t] = std::min(mv[t], h);
		}
	}
	for (uint32_t m = alive[X]; m; m &= m - 1) {
		int x = __builtin_ctz(m);
		int t = A.type[x];
		uint64_t p = S.pw[x];
		if (2 * p >= mw[t] || p >= mv[t]) return false;
	}
	return true;
}

template <bool part2>
uint32_t fight(const Army &A, State &S, uint32_t dmg_boost) {
	uint32_t alive[2] = { A.alive0[0], A.alive0[1] };
	uint32_t dm[NG];
	for (int i = 0; i < NG; i++) {
		dm[i] = A.dm[i] + ((alive[0] >> i) & 1) * dmg_boost;
		S.pw[i] = S.un[i] * dm[i];
	}

	uint32_t need[NG][NG];
	uint32_t hurts[NG];
	uint32_t killers[NG];
	if (part2) {
		for (uint32_t m = alive[1]; m; m &= m - 1) killers[__builtin_ctz(m)] = 0;
		for (uint32_t m = alive[0]; m; m &= m - 1) {
			int i = __builtin_ctz(m);
			int t = A.type[i];
			hurts[i] = 0;
			for (uint32_t c = alive[1] & A.vuln[t]; c; c &= c - 1) {
				int g = __builtin_ctz(c);
				uint64_t per = uint64_t(dm[i]) << ((A.wk[g] >> t) & 1);
				if (!per) continue;
				uint64_t q = (A.hp[g] + per - 1) / per;
				need[i][g] = q;
				if (q <= S.un[i]) {
					hurts[i] |= 1u << g;
					killers[g] |= 1u << i;
				}
			}
		}
		for (uint32_t m = alive[1]; m; m &= m - 1) {
			if (!killers[__builtin_ctz(m)]) return 0;
		}
	}

	uint8_t ord[NG], pos[NG];
	int n = 0;
	for (int i = 0; i < NG; i++) if (S.pw[i]) ord[n++] = i;
	auto less = [&](int a, int b) {
		return (S.pw[a] == S.pw[b]) ? a < b : S.pw[a] > S.pw[b];
	};
	for (int i = 1; i < n; i++) {
		for (int j = i; j > 0 && less(ord[j], ord[j - 1]); j--) std::swap(ord[j], ord[j - 1]);
	}
	uint32_t RM[NM] = { };
	for (int r = 0; r < n; r++) {
		int g = ord[r];
		pos[g] = r;
		uint32_t mb = A.memb[g];
		for (int q = 0; q < NM; q++) RM[q] |= ((mb >> q) & 1) << r;
	}

	uint8_t tgt[NG];

	for (;;) {
		if (!alive[0] || !alive[1]) break;

		uint32_t marks[2] = { RM[0], RM[1] };
		uint32_t attackers = 0;
		for (int i = 0; i < n; i++) {
			int a = ord[i];
			int e = !A.fac[a];
			int t = A.type[a];
			uint32_t m = marks[e];
			uint32_t w = m & RM[2 + t];
			m = w ? w : (m & RM[2 + N_TYPES + t]);
			if (!m) continue;
			int r = __builtin_ctz(m);
			tgt[a] = ord[r];
			attackers |= 1u << a;
			marks[e] ^= 1u << r;
		}

		uint32_t kf = 0;
		for (uint32_t m = attackers; m; ) {
			int a = __builtin_ctz(m);
			m &= m - 1;
			int d = tgt[a];
			uint32_t dmg = S.pw[a] << ((A.wk[d] >> A.type[a]) & 1);
			uint32_t kills = std::min(dmg / A.hp[d], S.un[d]);
			kf |= uint32_t(kills != 0) << A.fac[a];
			uint32_t u = S.un[d] -= kills;
			if (part2 && !A.fac[d] && kills) {
				for (uint32_t c = hurts[d]; c; c &= c - 1) {
					int g = __builtin_ctz(c);
					if (u < need[d][g]) {
						hurts[d] &= ~(1u << g);
						killers[g] &= ~(1u << d);
						if (!killers[g] && ((alive[1] >> g) & 1)) return 0;
					}
				}
			}
			uint32_t p = S.pw[d] = u * dm[d];
			int i0 = pos[d], i = i0;
			if (!p) {
				alive[A.fac[d]] &= ~(1u << d);
				m &= ~(1u << d);
				for (; i + 1 < n; i++) {
					ord[i] = ord[i + 1];
					pos[ord[i]] = i;
				}
				n--;
				uint32_t lo = (1u << i0) - 1;
				for (int q = 0; q < NM; q++) RM[q] = (RM[q] & lo) | ((RM[q] >> 1) & ~lo);
			} else if (i0 + 1 < n && (S.pw[ord[i0 + 1]] > p || (S.pw[ord[i0 + 1]] == p && ord[i0 + 1] < d))) {
				for (; i + 1 < n; i++) {
					int b = ord[i + 1];
					uint32_t pb = S.pw[b];
					if (pb > p || (pb == p && b < d)) {
						ord[i] = b;
						pos[b] = i;
					} else {
						break;
					}
				}
				if (i != i0) {
					ord[i] = d;
					pos[d] = i;
					uint32_t range = ((2u << i) - 1) & ~((1u << i0) - 1);
					uint32_t mid = range & ~(1u << i0);
					for (int q = 0; q < NM; q++) {
						uint32_t x = RM[q];
						uint32_t b = (x >> i0) & 1;
						RM[q] = (x & ~range) | ((x & mid) >> 1) | (b << i);
					}
				}
			}
		}

		if (!kf) return 0;

		if (kf != 3 && alive[0] && alive[1]) {
			int X = kf & 1, Y = !X;
			if (harmless(A, S, alive, X)) {
				uint8_t yo[NG];
				int ny = 0;
				for (int i = 0; i < n; i++) if (A.fac[ord[i]] == Y) yo[ny++] = ord[i];
				for (;;) {
					uint32_t marks = alive[X];
					uint32_t any = 0;
					for (int j = 0; j < ny; j++) {
						int y = yo[j];
						int t = A.type[y];
						uint32_t m = marks & A.weak[t];
						if (!m) m = marks & A.vuln[t];
						if (!m) continue;
						uint32_t best_p = 0;
						int d = 0;
						do {
							int c = __builtin_ctz(m);
							m &= m - 1;
							if (S.pw[c] > best_p) {
								best_p = S.pw[c];
								d = c;
							}
						} while (m);
						marks ^= 1u << d;
						uint32_t dmg = S.pw[y] << ((A.wk[d] >> t) & 1);
						uint32_t kills = std::min(dmg / A.hp[d], S.un[d]);
						any |= kills;
						S.un[d] -= kills;
						S.pw[d] = S.un[d] * dm[d];
						if (!S.pw[d]) alive[X] &= ~(1u << d);
					}
					if (!any) return 0;
					if (!alive[X]) break;
				}
				break;
			}
		}
	}

	if (part2 & !alive[0]) return 0;

	uint32_t result = 0;
	for (int i = 0; i < NG; i++) result += S.un[i];
	return result;
}
}

void day24(input_t input) {
	Army A = { };
	State S0 = { };

	uint32_t n = 0, faction = 0, k = 0;
	uint16_t last = 0;
	uint32_t types[3] = { 0, 0, 0 };
	uint32_t units = 0, hp = 0, dmg = 0;
	for (; input.len--; input.s++) {
		last = (last << 8) | uint8_t(*input.s);
		uint8_t c = *input.s - '0';
		if (c < 10) {
			n = 10 * n + c;
		} else if (*input.s == '\n' && n) {
			int i = 20 - n;
			A.alive0[faction] |= 1u << i;
			A.fac[i] = faction;
			A.type[i] = __builtin_ctz(types[0] | 32);
			A.wk[i] = types[1];
			A.hp[i] = hp;
			A.dm[i] = dmg;
			S0.un[i] = units;
			uint32_t imm = types[2];
			for (int t = 0; t < N_TYPES; t++) {
				if (types[1] >> t & 1) A.weak[t] |= 1u << i;
				if (!(imm >> t & 1)) A.vuln[t] |= 1u << i;
			}
			A.memb[i] = (1u << faction) | ((types[1] & 31) << 2) | ((~imm & 31) << (2 + N_TYPES));
			types[0] = types[1] = types[2] = 0;
			n = 0;
		} else switch (last) {
		    case tag(" u"): units = n; n = 0; break;
		    case tag(" h"): hp    = n; n = 0; break;
		    case tag("da"): dmg   = n; n = 0; break;
		    case tag("bl"): types[k] |=  1; break;
		    case tag("co"): types[k] |=  2; break;
		    case tag("fi"): types[k] |=  4; break;
		    case tag("ra"): types[k] |=  8; break;
		    case tag("sl"): types[k] |= 16; break;
		    case tag("tt"): k = 0; break;
		    case tag("we"): k = 1; break;
		    case tag("im"): k = 2; break;
		    case tag("n:"): faction = 1; break;
		}
	}

	State S = S0;
	uint32_t part1 = fight<false>(A, S, 0), part2 = 0;
	for (uint32_t boost = 1; !part2; boost++) {
		S = S0;
		part2 = fight<true>(A, S, boost);
	}

	printf("Day 24 Part 1: %u\nDay 24 Part 2: %u\n", part1, part2);
}
