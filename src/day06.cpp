#include "advent2018.h"
#include <arm_neon.h>

static constexpr int SAFE = 10000;

namespace {

struct Pt {
	int x, y;
};

constexpr int16_t VIRTUAL_Y = -16000;

static int distance_sums(const int *cnt, int len, int n, int sum, int *mid) {
	int distance_sum = sum, delta = n;
	int m = 0;
	for (int k = 0; k < len; k++) {
		mid[k] = distance_sum;
		if (distance_sum < mid[m]) m = k;
		delta -= cnt[k] * 2;
		distance_sum -= delta;
	}
	return m;
}

/* Processes 8*G consecutive rows of the Manhattan Voronoi diagram at once
 * (one row per 16-bit lane).  For each row, points sorted by x define
 * segments [px[k], px[k+1]); within a segment the nearest point is either
 * the prefix argmin of (h - px) or the suffix argmin of (h + px), with the
 * crossover at a computable x.  Area totals are accumulated per point
 * without scatters by exploiting that each point's runs are contiguous.
 */
template <int G, bool PARTIAL>
static void batch(int y0, int nl, int K, int z, const int16_t *kpx, const int16_t *kpy, const int16_t *kid,
		int16_t *vPA, int16_t *vPI, int16_t *vSB, int16_t *vSI, int16_t *vCL,
		int32x4_t *ACC, int32x4_t &tieAcc, int *area, uint8_t *inf) {
	constexpr int L = 8 * G;
	static const int16_t lane_init[8] = {0, 1, 2, 3, 4, 5, 6, 7};
	const int16x8_t lanes = vld1q_s16(lane_init);
	int16_t vX0[L];
	int16x8_t yv[G], curA[G], curIA[G], curB[G], curIB[G];
	uint16x8_t valid[G];
	for (int g = 0; g < G; g++) {
		yv[g] = vaddq_s16(vdupq_n_s16((int16_t)(y0 + 8 * g)), lanes);
		curA[g] = curB[g] = vdupq_n_s16(INT16_MAX);
		curIA[g] = curIB[g] = vdupq_n_s16(0);
		valid[g] = vcltq_s16(vaddq_s16(lanes, vdupq_n_s16((int16_t)(8 * g))), vdupq_n_s16((int16_t)nl));
	}
	for (int k = 0, j = K - 1; k < K; k++, j--) {
		const int16x8_t pyk = vdupq_n_s16(kpy[k]), pxk = vdupq_n_s16(kpx[k]);
		const int16x8_t pyj = vdupq_n_s16(kpy[j]), pxj = vdupq_n_s16(kpx[j]);
		const int16x8_t idk = vdupq_n_s16(kid[k]), idj = vdupq_n_s16(kid[j]);
		for (int g = 0; g < G; g++) {
			int16x8_t av = vsubq_s16(vabdq_s16(yv[g], pyk), pxk);
			int16x8_t bv = vaddq_s16(vabdq_s16(yv[g], pyj), pxj);
			uint16x8_t ltA = vcltq_s16(av, curA[g]), leA = vcleq_s16(av, curA[g]);
			uint16x8_t ltB = vcltq_s16(bv, curB[g]), leB = vcleq_s16(bv, curB[g]);
			curIA[g] = vbslq_s16(leA, vandq_s16(vreinterpretq_s16_u16(ltA), idk), curIA[g]);
			curIB[g] = vbslq_s16(leB, vandq_s16(vreinterpretq_s16_u16(ltB), idj), curIB[g]);
			curA[g] = vminq_s16(curA[g], av);
			curB[g] = vminq_s16(curB[g], bv);
			vst1q_s16(&vPA[k * L + 8 * g], curA[g]);
			vst1q_s16(&vPI[k * L + 8 * g], curIA[g]);
			vst1q_s16(&vSB[j * L + 8 * g], curB[g]);
			vst1q_s16(&vSI[j * L + 8 * g], curIB[g]);
		}
	}
	const int16x8_t one = vdupq_n_s16(1);
	const int16x8_t zero = vdupq_n_s16(0);
	int16x8_t tie[G], clp[G], crp[G], fp[G];
	for (int g = 0; g < G; g++) tie[g] = clp[g] = crp[g] = fp[g] = zero;
	for (int k = 0; k + 1 < K; k++) {
		const int16x8_t lo = vdupq_n_s16(kpx[k]), hi = vdupq_n_s16(kpx[k + 1]);
		const int16x8_t idn = vdupq_n_s16(kid[k + 1]);
		int32x4_t acc = ACC[kid[k + 1]];
		for (int g = 0; g < G; g++) {
			int16x8_t D = vsubq_s16(vld1q_s16(&vSB[(k + 1) * L + 8 * g]), vld1q_s16(&vPA[k * L + 8 * g]));
			int16x8_t e1 = vminq_s16(vmaxq_s16(vshrq_n_s16(vaddq_s16(D, one), 1), lo), hi);
			int16x8_t e2 = vminq_s16(vmaxq_s16(vaddq_s16(vshrq_n_s16(D, 1), one), lo), hi);
			int16x8_t cl = vsubq_s16(e1, lo), cr = vsubq_s16(hi, e2);
			int16x8_t pi = vld1q_s16(&vPI[k * L + 8 * g]);
			int16x8_t si1 = vld1q_s16(&vSI[(k + 1) * L + 8 * g]);
			int16x8_t si0 = vld1q_s16(&vSI[k * L + 8 * g]);
			tie[g] = vaddq_s16(tie[g], vaddq_s16(vsubq_s16(e2, e1),
				vaddq_s16(vandq_s16(cl, vreinterpretq_s16_u16(vceqzq_s16(pi))),
				          vandq_s16(cr, vreinterpretq_s16_u16(vceqzq_s16(si1))))));
			vst1q_s16(&vCL[k * L + 8 * g], clp[g]);
			uint16x8_t same = vceqq_s16(si1, si0);
			if (k == 0) same = vdupq_n_u16(0);
			fp[g] = vbslq_s16(same, fp[g], crp[g]);
			crp[g] = vaddq_s16(crp[g], cr);
			clp[g] = vaddq_s16(clp[g], cl);
			uint16x8_t own = vceqq_s16(si1, idn);
			if (PARTIAL) own = vandq_u16(valid[g], own);
			acc = vpadalq_s16(acc, vandq_s16(vsubq_s16(crp[g], fp[g]), vreinterpretq_s16_u16(own)));
		}
		ACC[kid[k + 1]] = acc;
	}
	{
		int16x8_t E[G], pnext[G];
		for (int g = 0; g < G; g++) {
			E[g] = clp[g];
			pnext[g] = vld1q_s16(&vPI[(K - 1) * L + 8 * g]);
		}
		for (int k = K - 2; k >= 0; k--) {
			const int16x8_t idk = vdupq_n_s16(kid[k]);
			int32x4_t acc = ACC[kid[k]];
			for (int g = 0; g < G; g++) {
				int16x8_t pi = vld1q_s16(&vPI[k * L + 8 * g]);
				int16x8_t clk1 = k + 1 == K - 1 ? clp[g] : vld1q_s16(&vCL[(k + 1) * L + 8 * g]);
				E[g] = vbslq_s16(vceqq_s16(pnext[g], pi), E[g], clk1);
				uint16x8_t own = vceqq_s16(pi, idk);
				if (PARTIAL) own = vandq_u16(valid[g], own);
				acc = vpadalq_s16(acc, vandq_s16(vsubq_s16(E[g], vld1q_s16(&vCL[k * L + 8 * g])), vreinterpretq_s16_u16(own)));
				pnext[g] = pi;
			}
			ACC[kid[k]] = acc;
		}
	}
	for (int g = 0; g < G; g++) {
		if (PARTIAL) tie[g] = vandq_s16(tie[g], vreinterpretq_s16_u16(valid[g]));
		tieAcc = vpadalq_s16(tieAcc, tie[g]);
		int16x8_t id;
		if (z + 1 < K) {
			int16x8_t D = vsubq_s16(vld1q_s16(&vSB[(z + 1) * L + 8 * g]), vld1q_s16(&vPA[z * L + 8 * g]));
			uint16x8_t gt = vcgtzq_s16(D), eq = vceqzq_s16(D);
			id = vbslq_s16(gt, vld1q_s16(&vPI[z * L + 8 * g]), vbicq_s16(vld1q_s16(&vSI[(z + 1) * L + 8 * g]), vreinterpretq_s16_u16(eq)));
		} else {
			id = vld1q_s16(&vPI[z * L + 8 * g]);
		}
		vst1q_s16(&vX0[8 * g], id);
	}
	for (int l = 0; l < nl; l++) {
		inf[vX0[l]] = 1;
		int last = vPI[(K - 1) * L + l];
		inf[last] = 1;
		area[last] += 1;
	}
}

}

void day06(input_t input) {
	std::vector<Pt> P;
	P.reserve(64);

	for (int n = 0, have = 0, which = 0; input.len--; input.s++) {
		uint8_t c = *input.s - '0';
		if (c < 10) {
			n = 10 * n + c;
			have = 1;
		} else if (have) {
			if ((which = !which)) {
				P.push_back({n, 0});
			} else {
				P.back().y = n;
			}
			n = have = 0;
		}
	}

	int minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;
	for (auto &p : P) {
		minx = std::min(minx, p.x); maxx = std::max(maxx, p.x);
		miny = std::min(miny, p.y); maxy = std::max(maxy, p.y);
	}
	int W = maxx - minx + 1, H = maxy - miny + 1;
	const int N = (int)P.size();
	int sumx = 0, sumy = 0;
	for (auto &p : P) {
		p.x -= minx; p.y -= miny;
		sumx += p.x; sumy += p.y;
	}
	/* Both parts are symmetric under transposition; iterate over the shorter axis */
	if (H > W) {
		for (auto &p : P) std::swap(p.x, p.y);
		std::swap(W, H);
		std::swap(sumx, sumy);
	}

	/* Part 1: per-row Manhattan Voronoi via prefix/suffix minima */
	std::vector<Pt> S = P;
	std::sort(S.begin(), S.end(), [](const Pt &a, const Pt &b) {
		return a.x != b.x ? a.x < b.x : a.y < b.y;
	});
	std::vector<int> px(N), py(N), PA(N), PI(N), SB(N), SI(N);
	for (int i = 0; i < N; i++) { px[i] = S[i].x; py[i] = S[i].y; }

	std::vector<int> area(N + 3, 0);
	std::vector<uint8_t> inf(N + 3, 0);

	auto border_row = [&](int y) {
		int curA = INT_MAX, curIdA = 0, curB = INT_MAX, curIdB = 0;
		for (int k = 0, j = N - 1; k < N; k++, j--) {
			int a = std::abs(y - py[k]) - px[k];
			int b = std::abs(y - py[j]) + px[j];
			int nidA = a < curA ? k + 1 : (a == curA ? 0 : curIdA);
			int nidB = b < curB ? j + 1 : (b == curB ? 0 : curIdB);
			curA = std::min(curA, a);
			curB = std::min(curB, b);
			curIdA = nidA; curIdB = nidB;
			PA[k] = curA; PI[k] = curIdA;
			SB[j] = curB; SI[j] = curIdB;
		}
		for (int k = 0; k + 1 < N; k++) {
			int lo = px[k], hi = px[k + 1];
			int D = SB[k + 1] - PA[k];
			int e1 = std::min(std::max((D + 1) >> 1, lo), hi);
			int e2 = std::min(std::max((D >> 1) + 1, lo), hi);
			area[PI[k]] += e1 - lo;
			area[0] += e2 - e1;
			area[SI[k + 1]] += hi - e2;
			if (e1 > lo) inf[PI[k]] = 1;
			if (e2 > e1) inf[0] = 1;
			if (hi > e2) inf[SI[k + 1]] = 1;
		}
		area[PI[N - 1]] += 1;
		inf[PI[N - 1]] = 1;
	};
	border_row(0);
	if (H > 1) border_row(H - 1);

	{
		constexpr int BG = 2, BL = 8 * BG;
		const int KM = N + 2;
		std::vector<int16_t> vPA(KM * BL), vPI(KM * BL), vSB(KM * BL), vSI(KM * BL), vCL(KM * BL);
		std::vector<int16_t> kpx(KM), kpy(KM), kid(KM);
		std::vector<int16_t> tx(N), ty(N), ti(N);
		std::vector<int> preA(N + 1);
		std::vector<int32x4_t> ACC(N + 3, vdupq_n_s32(0));
		int32x4_t tieAcc = vdupq_n_s32(0);
		constexpr int BIG = 1 << 28;

		/* Points strictly dominated (another point is closer at every x)
		 * on every row of the batch can be dropped.  At the batch center
		 * row, slack = (best other distance to the point's own cell) - h;
		 * slack is 2-Lipschitz in y, so slack < -BL proves domination on
		 * all rows within BL/2.  Virtual sentinel points at x=0 and x=W-1
		 * (very far away) keep the segment coverage complete.
		 */
		auto run = [&](int y0, int nl, auto partial) {
			const int yc = y0 + BL / 2;
			int m = BIG;
			for (int j = 0; j < N; j++) {
				preA[j] = m;
				m = std::min(m, std::abs(yc - py[j]) - px[j]);
			}
			int sufB = BIG;
			int cnt = 0;
			for (int j = N - 1; j >= 0; j--) {
				int h = std::abs(yc - py[j]);
				int E = std::min(preA[j] + px[j], sufB - px[j]);
				if (E - h >= -BL) {
					tx[cnt] = (int16_t)px[j]; ty[cnt] = (int16_t)py[j]; ti[cnt] = (int16_t)(j + 1);
					cnt++;
				}
				sufB = std::min(sufB, h + px[j]);
			}
			int K = 0;
			kpx[K] = 0; kpy[K] = VIRTUAL_Y; kid[K] = (int16_t)(N + 1); K++;
			for (int i = cnt - 1; i >= 0; i--, K++) {
				kpx[K] = tx[i]; kpy[K] = ty[i]; kid[K] = ti[i];
			}
			kpx[K] = (int16_t)(W - 1); kpy[K] = VIRTUAL_Y; kid[K] = (int16_t)(N + 2); K++;
			int z = 0;
			while (z + 1 < K && kpx[z + 1] == 0) z++;
			batch<BG, decltype(partial)::value>(y0, nl, K, z, kpx.data(), kpy.data(), kid.data(),
				vPA.data(), vPI.data(), vSB.data(), vSI.data(), vCL.data(),
				ACC.data(), tieAcc, area.data(), inf.data());
		};
		int y0 = 1;
		for (; y0 + BL <= H - 1; y0 += BL) run(y0, BL, std::false_type());
		if (y0 < H - 1) run(y0, H - 1 - y0, std::true_type());
		for (int k = 1; k <= N; k++) area[k] += vaddvq_s32(ACC[k]);
		area[0] += vaddvq_s32(tieAcc);
	}

	int part1 = 0;
	for (int i = 0; i <= N; i++) {
		if (!inf[i]) part1 = std::max(part1, area[i]);
	}

	/* Part 2: distance sums separate into x and y components, each convex.
	 * Walk x outward from its minimum; the y-interval with sum below the
	 * threshold shrinks monotonically, tracked by two pointers.
	 */
	std::vector<int> xc(W, 0), yc(H, 0);
	for (auto &p : P) { xc[p.x]++; yc[p.y]++; }
	std::vector<int> xmid(W), ymid(H), ey(H + 2 * (SAFE / (N > 0 ? N : 1) + 2) + 32);
	const int mx = distance_sums(xc.data(), W, N, sumx, xmid.data());
	const int my = distance_sums(yc.data(), H, N, sumy, ymid.data());
	int ylo, yhi, ymin;
	{
		int *e = ey.data();
		int c = 0;
		for (int i = 0; i < 8; i++) e[c++] = SAFE;
		const int first = ymid[0], last = ymid[H - 1];
		const int kl = first < SAFE ? (SAFE - 1 - first) / N : 0;
		ylo = c;
		for (int k = kl; k >= 1; k--) e[c++] = first + k * N;
		ymin = c + my;
		for (int y = 0; y < H; y++) e[c++] = std::min(ymid[y], SAFE);
		const int kr = last < SAFE ? (SAFE - 1 - last) / N : 0;
		for (int k = 1; k <= kr; k++) e[c++] = last + k * N;
		yhi = c - 1;
		for (int i = 0; i < 8; i++) e[c++] = SAFE;
	}
	const int *EY = ey.data();
	const int eymin = EY[ymin];

	int part2 = 0;
	auto pass = [&](int xi, int dir) {
		int lo = ylo, hi = yhi;
		const int first = xmid[0], last = xmid[W - 1];
		while (true) {
			int v = xi < 0 ? first - xi * N : (xi >= W ? last + (xi - W + 1) * N : xmid[xi]);
			int T = SAFE - v;
			if (eymin >= T) break;
			while (EY[lo] >= T) lo++;
			while (EY[hi] >= T) hi--;
			part2 += hi - lo + 1;
			xi += dir;
		}
	};
	pass(mx, 1);
	pass(mx - 1, -1);

	printf("Day 06 Part 1: %d\nDay 06 Part 2: %d\n", part1, part2);
}
