#ifndef _ADVENT2018_H
#define _ADVENT2018_H

#include <string>
#include <array>
#include <vector>
#include <set>
#include <algorithm>
#include <cstdint>
#include <climits>
#include <cstring>
#include <unordered_map>
#include <functional>
#include <cmath>
#if defined(__x86_64__)
#include <x86intrin.h>
#else
#define SIMDE_ENABLE_NATIVE_ALIASES
#include <simde/x86/avx2.h>
#include <simde/x86/sse4.2.h>
static inline unsigned _tzcnt_u32(unsigned x) { return x ? __builtin_ctz(x) : 32; }
static inline int _mm_tzcnt_32(unsigned x) { return x ? __builtin_ctz(x) : 32; }
static inline long long _mm_tzcnt_64(unsigned long long x) { return x ? __builtin_ctzll(x) : 64; }
static inline unsigned _blsr_u32(unsigned x) { return x & (x - 1); }
static inline unsigned _blsi_u32(unsigned x) { return x & -x; }
static inline int _bit_scan_forward(int x) { return __builtin_ctz(x); }
static inline int _popcnt32(int x) { return __builtin_popcount(x); }
static inline long long _mm_popcnt_u64(unsigned long long x) { return __builtin_popcountll(x); }
#endif

/* Need this to safely do horrible things such as taking a
 * std::vector<int8_t> and casting it to __m256i for SIMD
 */
#include <boost/align/aligned_allocator.hpp>
using boost::alignment::aligned_allocator;

struct input_t {
	char *s;
	ssize_t len;
};

struct advent_t {
	void (*fn)(input_t);
	std::string input_file;
};

void day01(input_t);
void day02(input_t);
void day03(input_t);
void day04(input_t);
void day05(input_t);
void day06(input_t);
void day07(input_t);
void day08(input_t);
void day09(input_t);
void day10(input_t);
void day11(input_t);
void day12(input_t);
void day13(input_t);
void day14(input_t);
void day15(input_t);
void day16(input_t);
void day17(input_t);
void day18(input_t);
void day19(input_t);
void day20(input_t);
void day21(input_t);
void day22(input_t);
void day23(input_t);
void day24(input_t);
void day25(input_t);

#endif
