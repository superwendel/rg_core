// Shared timing and machine-readable output for the regression benchmarks.
#ifndef RG_BENCH_COMMON_H
#define RG_BENCH_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <intrin.h>
#else
#include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Compile bench_sink.c separately without LTCG so output consumption stays opaque.
uint64_t rg_bench_consume(const void* data, size_t bytes);
#ifdef __cplusplus
}
#endif

typedef double (*RgBenchSampleFn)(void* context, uint64_t* checksum);
static const char* rg_bench_filter;

static void rg_bench_init(int argc, char** argv)
{
	rg_bench_filter = argc > 1 ? argv[1] : NULL;
#ifdef _WIN32
	DWORD_PTR process_mask = 0;
	DWORD_PTR system_mask = 0;
	if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask))
	{
		DWORD_PTR chosen = process_mask & (~process_mask + 1);
		if ((process_mask & 4u) != 0) chosen = 4u;
		if (!SetThreadAffinityMask(GetCurrentThread(), chosen))
			fprintf(stderr, "Could not pin benchmark thread: %lu\n", GetLastError());
	}
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
#endif
}

static double rg_bench_now_ns(void)
{
#ifdef _WIN32
	static double scale;
	LARGE_INTEGER ticks;
	if (scale == 0.0)
	{
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		scale = 1000000000.0 / (double)frequency.QuadPart;
	}
	_ReadWriteBarrier();
	QueryPerformanceCounter(&ticks);
	_ReadWriteBarrier();
	return (double)ticks.QuadPart * scale;
#else
	struct timespec ticks;
	clock_gettime(CLOCK_MONOTONIC, &ticks);
	return (double)ticks.tv_sec * 1000000000.0 + (double)ticks.tv_nsec;
#endif
}

static int rg_bench_selected(const char* name)
{
	if (rg_bench_filter == NULL || strcmp(rg_bench_filter, "--list") == 0) return 1;
	if (rg_bench_filter[0] == '=') return strcmp(name, rg_bench_filter + 1) == 0;
	return strstr(name, rg_bench_filter) != NULL;
}

static void rg_bench_run(const char* name, RgBenchSampleFn sample, void* context)
{
	if (!rg_bench_selected(name)) return;
	if (rg_bench_filter != NULL && strcmp(rg_bench_filter, "--list") == 0)
	{
		printf("CASE %s\n", name);
		return;
	}
	uint64_t checksum = 0;
	(void)sample(context, &checksum);
	for (int i = 0; i < 7; ++i)
	{
		double ns = sample(context, &checksum);
		printf("BENCH %s %d %.9f %llu\n", name, i, ns,
		       (unsigned long long)checksum);
	}
}

#endif
