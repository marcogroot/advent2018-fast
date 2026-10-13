#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include "advent2018.h"

static constexpr size_t BACKSPLASH_SIZE = 1 << 20;

static void (*const days[])(input_t) = {
	day01, day02, day03, day04, day05, day06, day07, day08, day09, day10,
	day11, day12, day13, day14, day15, day16, day17, day18, day19, day20,
	day21, day22, day23, day24, day25,
};

static input_t load(const std::string &filename) {
	static void *backsplash = NULL;
	backsplash = mmap(backsplash, BACKSPLASH_SIZE, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS | (backsplash ? MAP_FIXED : 0), -1, 0);
	if (backsplash == MAP_FAILED) { perror("mmap"); exit(1); }
	int fd = open(filename.c_str(), O_RDONLY);
	if (fd == -1) { perror(filename.c_str()); exit(1); }
	input_t input;
	input.len = lseek(fd, 0, SEEK_END);
	input.s = reinterpret_cast<char *>(mmap(backsplash, input.len, PROT_READ, MAP_PRIVATE | MAP_FIXED, fd, 0));
	if (input.s == MAP_FAILED) { perror("mmap"); exit(1); }
	close(fd);
	return input;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <day|all> [input_file] [-n iters] [-q]\n", argv[0]);
		return 1;
	}
	std::string which = argv[1];
	std::string file;
	int iters = 0;
	bool quiet = false;
	for (int i = 2; i < argc; i++) {
		std::string a = argv[i];
		if (a == "-n" && i + 1 < argc) iters = atoi(argv[++i]);
		else if (a == "-q") quiet = true;
		else file = a;
	}
	std::vector<int> list;
	if (which == "all") for (int d = 1; d <= 25; d++) list.push_back(d);
	else list.push_back(atoi(which.c_str()));

	double total_min = 0, total_med = 0;
	for (int d : list) {
		char buf[64];
		snprintf(buf, sizeof buf, "input/day%02d.txt", d);
		input_t input = load(file.empty() || list.size() > 1 ? std::string(buf) : file);

		fflush(stdout);
		int saved = dup(1);
		if (quiet) { int nul = open("/dev/null", O_WRONLY); dup2(nul, 1); close(nul); }
		days[d - 1](input);
		fflush(stdout);
		int nul = open("/dev/null", O_WRONLY);
		dup2(nul, 1);
		close(nul);

		std::vector<double> t;
		auto start = std::chrono::steady_clock::now();
		int n = iters;
		for (int i = 0; n ? i < n : (i < 10000 && (i < 5 || std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500))); i++) {
			auto t0 = std::chrono::steady_clock::now();
			days[d - 1](input);
			auto t1 = std::chrono::steady_clock::now();
			t.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
		}
		fflush(stdout);
		dup2(saved, 1);
		close(saved);
		std::sort(t.begin(), t.end());
		double mn = t[0], med = t[t.size() / 2];
		total_min += mn;
		total_med += med;
		fprintf(stderr, "Day %02d  min %10.1f us  median %10.1f us  (%zu runs)\n", d, mn, med, t.size());
		munmap(input.s, input.len);
	}
	if (list.size() > 1) fprintf(stderr, "Total   min %10.1f us  median %10.1f us\n", total_min, total_med);
	return 0;
}
