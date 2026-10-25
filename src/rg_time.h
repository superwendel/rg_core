// rg_time - Monotonic timing and thread sleep helpers
//
// Part of the Reverse Gravity (rg_) core libraries.
// Single-header C99 library for high-resolution monotonic timing, tick
// conversion, sleeping, and yielding.
//
// USAGE:
//   #include "rg_time.h"
//
//   rg_time_init(); // Before starting worker threads
//   u64 start = rg_time_ticks();
//   // work...
//   f64 elapsed_ms = rg_time_ticks_to_ms(rg_time_ticks() - start);
//
// OPTIONS:
//   #define RG_TIME_CUSTOM 1            - Use custom platform hooks
//   #define RG_TIME_PLATFORM_TICKS      - Custom tick function
//   #define RG_TIME_PLATFORM_FREQUENCY  - Custom frequency function
//   #define RG_TIME_PLATFORM_SLEEP_NS   - Custom nanosecond sleep function
//   #define RG_TIME_PLATFORM_YIELD      - Custom yield function
//   #define RG_TIME_USE_TSC 0           - Force the OS clock on x86 (default: 1 on x86-64)
//   #define RG_TIME_TSC_CALIBRATE_NS    - OS-clock calibration window when CPUID
//                                         leaf 0x15 is unavailable (default: 10 ms)
//
// NOTES:
//   - Time values are monotonic, not wall-clock timestamps.
//   - Call rg_time_init once per translation unit to cache the frequency.
//   - On x86-64 the first timing call (normally rg_time_init) selects the
//     rdtsc backend when CPUID reports an invariant TSC and calibrates it;
//     otherwise the OS clock is used. See docs/rg_time.md for the caveats.
//   - Custom hook macros name functions and are invoked by this header.
//   - All functions have internal linkage and work in unity builds.
//
// Author: Steven Wendel (superwendel)

#ifndef RG_TIME_H
#define RG_TIME_H

#include "rg_defs.h"

#include <stdint.h>

#ifndef RG_TIME_CUSTOM
#define RG_TIME_CUSTOM 0
#endif

// The rdtsc backend needs an x86 compiler with cpuid/rdtsc intrinsics.
#if (RG_ARCH_X64 || RG_ARCH_X86) && (RG_COMPILER_MSVC || RG_COMPILER_GCC || RG_COMPILER_CLANG)
#define RG_TIME_TSC_SUPPORTED 1
#else
#define RG_TIME_TSC_SUPPORTED 0
#endif

#ifndef RG_TIME_USE_TSC
#if RG_TIME_TSC_SUPPORTED && RG_ARCH_X64 && !RG_TIME_CUSTOM
#define RG_TIME_USE_TSC 1
#else
#define RG_TIME_USE_TSC 0
#endif
#endif

#if RG_TIME_USE_TSC != 0 && RG_TIME_USE_TSC != 1
#error RG_TIME_USE_TSC must be 0 or 1
#endif
#if RG_TIME_USE_TSC && !RG_TIME_TSC_SUPPORTED
#error RG_TIME_USE_TSC requires an x86 target with GCC, Clang, or MSVC
#endif
#if RG_TIME_USE_TSC && RG_TIME_CUSTOM
#error RG_TIME_USE_TSC cannot be combined with RG_TIME_CUSTOM
#endif

#ifndef RG_TIME_TSC_CALIBRATE_NS
#define RG_TIME_TSC_CALIBRATE_NS UINT64_C(10000000)
#endif
#if RG_TIME_USE_TSC && (RG_TIME_TSC_CALIBRATE_NS) <= 0
#error RG_TIME_TSC_CALIBRATE_NS must be a positive nanosecond count
#endif

#if RG_TIME_USE_TSC
#if RG_COMPILER_MSVC
#include <intrin.h>
#else
#include <cpuid.h>
#include <x86intrin.h>
#endif
#endif

#if RG_TIME_CUSTOM
#ifndef RG_TIME_PLATFORM_TICKS
#error RG_TIME_CUSTOM requires RG_TIME_PLATFORM_TICKS
#endif
#ifndef RG_TIME_PLATFORM_FREQUENCY
#error RG_TIME_CUSTOM requires RG_TIME_PLATFORM_FREQUENCY
#endif
#ifndef RG_TIME_PLATFORM_SLEEP_NS
#error RG_TIME_CUSTOM requires RG_TIME_PLATFORM_SLEEP_NS
#endif
#ifndef RG_TIME_PLATFORM_YIELD
#error RG_TIME_CUSTOM requires RG_TIME_PLATFORM_YIELD
#endif

RGINLINE int rg_time_using_tsc(void)
{
	return 0;
}
#else
#ifndef RG_TIME_PLATFORM_TICKS
#define RG_TIME_PLATFORM_TICKS rg_time_platform_ticks
#endif
#ifndef RG_TIME_PLATFORM_FREQUENCY
#define RG_TIME_PLATFORM_FREQUENCY rg_time_platform_frequency
#endif
#ifndef RG_TIME_PLATFORM_SLEEP_NS
#define RG_TIME_PLATFORM_SLEEP_NS rg_time_platform_sleep_ns
#endif
#ifndef RG_TIME_PLATFORM_YIELD
#define RG_TIME_PLATFORM_YIELD rg_time_platform_yield
#endif

#if RG_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif RG_PLATFORM_LINUX
#include <errno.h>
#include <sched.h>
#include <time.h>
#elif RG_PLATFORM_MACOS
#include <errno.h>
#include <mach/mach_time.h>
#include <sched.h>
#include <time.h>
#else
#error rg_time requires RG_TIME_CUSTOM on unsupported platforms
#endif
#endif

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Cache the platform timer frequency for fast duration conversions
 * @note Call once per translation unit before starting threads that use the
 *       conversion or convenience functions. Repeated calls are inexpensive.
 */
RGINLINE void rg_time_init(void);

/**
 * @brief Return the current monotonic timer tick count
 * @return Monotonic timestamp in platform ticks
 * @note With the rdtsc backend, ticks are TSC counts and the frequency is the
 *       calibrated TSC rate; always convert with the rg_time helpers.
 */
RGINLINE u64 rg_time_ticks(void);

/**
 * @brief Return the monotonic timer frequency
 * @return Platform ticks per second, or zero if unavailable
 */
RGINLINE u64 rg_time_ticks_per_second(void);

/**
 * @brief Convert a tick duration to seconds
 * @param ticks Duration in platform ticks
 * @return Duration in seconds, or zero when frequency is unavailable
 */
RGINLINE f64 rg_time_ticks_to_seconds(u64 ticks);

/**
 * @brief Convert a tick duration to milliseconds
 * @param ticks Duration in platform ticks
 * @return Duration in milliseconds, or zero when frequency is unavailable
 */
RGINLINE f64 rg_time_ticks_to_ms(u64 ticks);

/**
 * @brief Convert a tick duration to microseconds
 * @param ticks Duration in platform ticks
 * @return Duration in microseconds, or zero when frequency is unavailable
 */
RGINLINE f64 rg_time_ticks_to_us(u64 ticks);

/**
 * @brief Convert seconds to platform ticks
 * @param seconds Duration in seconds
 * @return Duration in ticks, saturated to UINT64_MAX
 */
RGINLINE u64 rg_time_seconds_to_ticks(f64 seconds);

/**
 * @brief Convert milliseconds to platform ticks
 * @param ms Duration in milliseconds
 * @return Duration in ticks, saturated to UINT64_MAX
 */
RGINLINE u64 rg_time_ms_to_ticks(f64 ms);

/**
 * @brief Convert microseconds to platform ticks
 * @param us Duration in microseconds
 * @return Duration in ticks, saturated to UINT64_MAX
 */
RGINLINE u64 rg_time_us_to_ticks(f64 us);

/**
 * @brief Return monotonic time in seconds
 * @return Monotonic time in seconds
 */
RGINLINE f64 rg_time_seconds(void);

/**
 * @brief Return monotonic time in milliseconds
 * @return Monotonic time in milliseconds
 */
RGINLINE f64 rg_time_ms(void);

/**
 * @brief Return monotonic time in microseconds
 * @return Monotonic time in microseconds
 */
RGINLINE f64 rg_time_us(void);

/**
 * @brief Sleep for at least the requested number of nanoseconds
 * @param ns Requested duration in nanoseconds
 */
RGINLINE void rg_time_sleep_ns(u64 ns);

/**
 * @brief Sleep for at least the requested number of microseconds
 * @param us Requested duration in microseconds
 */
RGINLINE void rg_time_sleep_us(u64 us);

/**
 * @brief Sleep for at least the requested number of milliseconds
 * @param ms Requested duration in milliseconds
 */
RGINLINE void rg_time_sleep_ms(u32 ms);

/**
 * @brief Yield execution to another runnable thread
 */
RGINLINE void rg_time_yield(void);

/**
 * @brief Report whether the rdtsc backend is active
 * @return 1 when rg_time_ticks reads the TSC, 0 when it uses the OS or custom clock
 * @note Probes the hardware on first use, like rg_time_init.
 */
RGINLINE int rg_time_using_tsc(void);

// =============================================================================
// PLATFORM BACKENDS
// =============================================================================

#if !RG_TIME_CUSTOM
RGINLINE u64 rg_time_os_ticks(void);
RGINLINE u64 rg_time_os_frequency(void);
RGINLINE u64 rg_time_platform_ticks(void);
RGINLINE u64 rg_time_platform_frequency(void);
RGINLINE void rg_time_platform_sleep_ns(u64 ns);
RGINLINE void rg_time_platform_yield(void);

#if RG_PLATFORM_WINDOWS

RGINLINE u64 rg_time_os_ticks(void)
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return (u64)counter.QuadPart;
}

RGINLINE u64 rg_time_os_frequency(void)
{
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	return (u64)frequency.QuadPart;
}

RGINLINE void rg_time_platform_sleep_ns(u64 ns)
{
	if (ns == 0u) return;

	u64 ms = ns / UINT64_C(1000000);
	if (ns % UINT64_C(1000000) != 0u) ms++;
	DWORD platform_ms = ms > UINT32_MAX ? UINT32_MAX : (DWORD)ms;
	Sleep(platform_ms);
}

RGINLINE void rg_time_platform_yield(void)
{
	SwitchToThread();
}

#elif RG_PLATFORM_LINUX

RGINLINE u64 rg_time_os_ticks(void)
{
	struct timespec timestamp;
#ifdef CLOCK_MONOTONIC_RAW
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &timestamp) != 0) return 0;
#else
	if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) return 0;
#endif
	return (u64)timestamp.tv_sec * UINT64_C(1000000000) +
	       (u64)timestamp.tv_nsec;
}

RGINLINE u64 rg_time_os_frequency(void)
{
	return UINT64_C(1000000000);
}

RGINLINE void rg_time_platform_sleep_ns(u64 ns)
{
	if (ns == 0u) return;

	struct timespec request;
	request.tv_sec = (time_t)(ns / UINT64_C(1000000000));
	request.tv_nsec = (long)(ns % UINT64_C(1000000000));
	while (nanosleep(&request, &request) != 0 && errno == EINTR)
	{
	}
}

RGINLINE void rg_time_platform_yield(void)
{
	sched_yield();
}

#elif RG_PLATFORM_MACOS

RGINLINE u64 rg_time_os_ticks(void)
{
	return (u64)mach_absolute_time();
}

RGINLINE u64 rg_time_os_frequency(void)
{
	mach_timebase_info_data_t info;
	if (mach_timebase_info(&info) != KERN_SUCCESS || info.numer == 0u || info.denom == 0u)
	{
		return 0;
	}
	return (UINT64_C(1000000000) * (u64)info.denom) / (u64)info.numer;
}

RGINLINE void rg_time_platform_sleep_ns(u64 ns)
{
	if (ns == 0u) return;

	struct timespec request;
	request.tv_sec = (time_t)(ns / UINT64_C(1000000000));
	request.tv_nsec = (long)(ns % UINT64_C(1000000000));
	while (nanosleep(&request, &request) != 0 && errno == EINTR)
	{
	}
}

RGINLINE void rg_time_platform_yield(void)
{
	sched_yield();
}

#endif

// -----------------------------------------------------------------------------
// TSC backend
// -----------------------------------------------------------------------------
// rdtsc costs a fraction of an OS clock read. It is used only when CPUID leaf
// 0x80000007 reports an invariant TSC (constant rate across P/C-states). The
// rate comes from CPUID leaf 0x15 when it reports a crystal frequency, and is
// otherwise measured against the OS clock over RG_TIME_TSC_CALIBRATE_NS using
// the tightest of several rdtsc/OS-clock sample brackets at each end.

#if RG_TIME_USE_TSC

// 0 = not yet probed, 2 = probe in progress, 1 = rdtsc selected,
// -1 = OS clock selected. The probe runs once: the first caller claims the
// state with a compare-and-swap, writes the frequency, then publishes the
// result with a release store; concurrent first callers wait for it.
static i32 rg_time_tsc_state;
static u64 rg_time_tsc_hz;

#if RG_COMPILER_MSVC
RGINLINE i32 rg_time_tsc_load_state(void)
{
	i32 state = *(volatile i32*)&rg_time_tsc_state;
	_ReadWriteBarrier();
	return state;
}
RGINLINE i32 rg_time_tsc_load_state_relaxed(void)
{
	return *(volatile i32*)&rg_time_tsc_state;
}
RGINLINE int rg_time_tsc_claim_probe(void)
{
	return _InterlockedCompareExchange((volatile long*)&rg_time_tsc_state, 2L, 0L) == 0L;
}
RGINLINE void rg_time_tsc_publish(i32 state)
{
	_ReadWriteBarrier();
	*(volatile i32*)&rg_time_tsc_state = state;
}
#else
RGINLINE i32 rg_time_tsc_load_state(void)
{
	return __atomic_load_n(&rg_time_tsc_state, __ATOMIC_ACQUIRE);
}
RGINLINE i32 rg_time_tsc_load_state_relaxed(void)
{
	return __atomic_load_n(&rg_time_tsc_state, __ATOMIC_RELAXED);
}
RGINLINE int rg_time_tsc_claim_probe(void)
{
	i32 expected = 0;
	return __atomic_compare_exchange_n(&rg_time_tsc_state, &expected, 2, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}
RGINLINE void rg_time_tsc_publish(i32 state)
{
	__atomic_store_n(&rg_time_tsc_state, state, __ATOMIC_RELEASE);
}
#endif

RGINLINE void rg_time_cpuid(u32 leaf, u32 subleaf, u32 out[4])
{
#if RG_COMPILER_MSVC
	int registers[4];
	__cpuidex(registers, (int)leaf, (int)subleaf);
	out[0] = (u32)registers[0];
	out[1] = (u32)registers[1];
	out[2] = (u32)registers[2];
	out[3] = (u32)registers[3];
#else
	u32 eax = 0, ebx = 0, ecx = 0, edx = 0;
	__cpuid_count(leaf, subleaf, eax, ebx, ecx, edx);
	out[0] = eax;
	out[1] = ebx;
	out[2] = ecx;
	out[3] = edx;
#endif
}

RGINLINE u64 rg_time_tsc_read(void)
{
	return (u64)__rdtsc();
}

// Capture matching OS-clock and TSC readings. Of several attempts the one
// with the tightest rdtsc bracket around the OS read is kept, which rejects
// preemption and cache misses and leaves an uncertainty of a few nanoseconds.
RGINLINE void rg_time_tsc_sample(u64* os_out, u64* tsc_out)
{
	u64 best_spread = UINT64_MAX;
	*os_out = 0u;
	*tsc_out = 0u;
	for (u32 attempt = 0; attempt < 16u; attempt++)
	{
		u64 before = rg_time_tsc_read();
		u64 os = rg_time_os_ticks();
		u64 after = rg_time_tsc_read();
		u64 spread = after - before;
		if (spread < best_spread)
		{
			best_spread = spread;
			*os_out = os;
			*tsc_out = before + spread / 2u;
		}
	}
}

// Probe the CPU and calibrate. Returns 1 with *hz_out set when rdtsc is
// usable, -1 otherwise. Called once, by the thread that claimed the probe.
static RG_NOINLINE i32 rg_time_tsc_detect(u64* hz_out)
{
	u32 registers[4];
	u64 hz = 0u;

#if RG_COMPILER_MSVC
	rg_time_cpuid(0u, 0u, registers);
	u32 max_basic_leaf = registers[0];
	rg_time_cpuid(0x80000000u, 0u, registers);
	u32 max_extended_leaf = registers[0];
#else
	u32 max_basic_leaf = (u32)__get_cpuid_max(0u, NULL);
	u32 max_extended_leaf = (u32)__get_cpuid_max(0x80000000u, NULL);
#endif

	// Invariant TSC: leaf 0x80000007 EDX bit 8.
	if (max_extended_leaf < 0x80000007u) return -1;
	rg_time_cpuid(0x80000007u, 0u, registers);
	if ((registers[3] & (1u << 8)) == 0u) return -1;

	// Leaf 0x15: EAX = denominator, EBX = numerator, ECX = crystal Hz (may be 0).
	// RG_TIME_TSC_FORCE_CALIBRATION is a test hook that exercises the OS-clock
	// calibration on hardware that reports the ratio.
#if !defined(RG_TIME_TSC_FORCE_CALIBRATION)
	if (max_basic_leaf >= 0x15u)
	{
		rg_time_cpuid(0x15u, 0u, registers);
		if (registers[0] != 0u && registers[1] != 0u && registers[2] != 0u)
			hz = (u64)registers[2] * (u64)registers[1] / (u64)registers[0];
	}
#else
	RG_UNUSED(max_basic_leaf);
#endif

	if (hz == 0u)
	{
		u64 os_frequency = rg_time_os_frequency();
		if (os_frequency == 0u) return -1;
		u64 window_ticks = (u64)((f64)RG_TIME_TSC_CALIBRATE_NS * (f64)os_frequency / 1000000000.0);
		if (window_ticks == 0u) window_ticks = 1u;

		u64 os_start = 0u, tsc_start = 0u, os_end = 0u, tsc_end = 0u;
		rg_time_tsc_sample(&os_start, &tsc_start);
		for (;;)
		{
			u64 elapsed = rg_time_os_ticks() - os_start;
			if (elapsed >= window_ticks) break;
			u64 remaining_ns = (u64)((f64)(window_ticks - elapsed) * 1000000000.0 / (f64)os_frequency);
			rg_time_platform_sleep_ns(remaining_ns != 0u ? remaining_ns : 1u);
		}
		rg_time_tsc_sample(&os_end, &tsc_end);
		if (os_end <= os_start || tsc_end <= tsc_start) return -1;
		hz = (u64)((f64)(tsc_end - tsc_start) * (f64)os_frequency / (f64)(os_end - os_start) + 0.5);
		if (hz == 0u) return -1;
	}

	*hz_out = hz;
	return 1;
}

// Returns the published backend state (1 or -1), running the probe exactly
// once. Threads that arrive while another thread probes wait for its result,
// so every thread observes the same backend and frequency.
static RG_NOINLINE i32 rg_time_tsc_ensure(void)
{
	i32 state = rg_time_tsc_load_state();
	if (state == 0 && rg_time_tsc_claim_probe())
	{
		u64 hz = 0u;
		state = rg_time_tsc_detect(&hz);
		rg_time_tsc_hz = hz;
		rg_time_tsc_publish(state);
		return state;
	}
	while (state == 0 || state == 2)
	{
		rg_time_yield();
		state = rg_time_tsc_load_state();
	}
	return state;
}

static RG_NOINLINE u64 rg_time_tsc_ticks_slow(void)
{
	return rg_time_tsc_ensure() == 1 ? rg_time_tsc_read() : rg_time_os_ticks();
}

RGINLINE u64 rg_time_platform_ticks(void)
{
	// A relaxed atomic read (a plain mov on x86) is enough here: once published
	// the state never changes, and the rdtsc path has no data that depends on it.
	if (RG_LIKELY(rg_time_tsc_load_state_relaxed() == 1)) return rg_time_tsc_read();
	return rg_time_tsc_ticks_slow();
}

RGINLINE u64 rg_time_platform_frequency(void)
{
	return rg_time_tsc_ensure() == 1 ? rg_time_tsc_hz : rg_time_os_frequency();
}

RGINLINE int rg_time_using_tsc(void)
{
	return rg_time_tsc_ensure() == 1;
}

#else

RGINLINE u64 rg_time_platform_ticks(void)
{
	return rg_time_os_ticks();
}

RGINLINE u64 rg_time_platform_frequency(void)
{
	return rg_time_os_frequency();
}

RGINLINE int rg_time_using_tsc(void)
{
	return 0;
}

#endif
#endif

// =============================================================================
// IMPLEMENTATION
// =============================================================================

static u64 rg_time_frequency_cache;

RGINLINE void rg_time_init(void)
{
	if (rg_time_frequency_cache != 0u) return;
	u64 frequency = (u64)RG_TIME_PLATFORM_FREQUENCY();
	if (frequency != 0u) rg_time_frequency_cache = frequency;
}

RGINLINE u64 rg_time_ticks(void)
{
	return (u64)RG_TIME_PLATFORM_TICKS();
}

RGINLINE u64 rg_time_ticks_per_second(void)
{
	u64 frequency = rg_time_frequency_cache;
	return frequency != 0u ? frequency : (u64)RG_TIME_PLATFORM_FREQUENCY();
}

RGINLINE f64 rg_time_ticks_to_seconds(u64 ticks)
{
	u64 frequency = rg_time_ticks_per_second();
	if (frequency == 0u) return 0.0;
	return (f64)ticks / (f64)frequency;
}

RGINLINE f64 rg_time_ticks_to_ms(u64 ticks)
{
	u64 frequency = rg_time_ticks_per_second();
	if (frequency == 0u) return 0.0;
	return (f64)ticks * 1000.0 / (f64)frequency;
}

RGINLINE f64 rg_time_ticks_to_us(u64 ticks)
{
	u64 frequency = rg_time_ticks_per_second();
	if (frequency == 0u) return 0.0;
	return (f64)ticks * 1000000.0 / (f64)frequency;
}

RGINLINE u64 rg_time_units_to_ticks(f64 value, f64 units_per_second)
{
	if (!(value > 0.0)) return 0;

	u64 frequency = rg_time_ticks_per_second();
	if (frequency == 0u) return 0;

	f64 ticks = value * (f64)frequency / units_per_second;
	if (ticks >= (f64)UINT64_MAX) return UINT64_MAX;
	return (u64)ticks;
}

RGINLINE u64 rg_time_seconds_to_ticks(f64 seconds)
{
	return rg_time_units_to_ticks(seconds, 1.0);
}

RGINLINE u64 rg_time_ms_to_ticks(f64 ms)
{
	return rg_time_units_to_ticks(ms, 1000.0);
}

RGINLINE u64 rg_time_us_to_ticks(f64 us)
{
	return rg_time_units_to_ticks(us, 1000000.0);
}

RGINLINE f64 rg_time_seconds(void)
{
	return rg_time_ticks_to_seconds(rg_time_ticks());
}

RGINLINE f64 rg_time_ms(void)
{
	return rg_time_ticks_to_ms(rg_time_ticks());
}

RGINLINE f64 rg_time_us(void)
{
	return rg_time_ticks_to_us(rg_time_ticks());
}

RGINLINE void rg_time_sleep_ns(u64 ns)
{
	if (ns == 0u) return;
	RG_TIME_PLATFORM_SLEEP_NS(ns);
}

RGINLINE void rg_time_sleep_us(u64 us)
{
	u64 ns = us > UINT64_MAX / UINT64_C(1000) ? UINT64_MAX : us * UINT64_C(1000);
	rg_time_sleep_ns(ns);
}

RGINLINE void rg_time_sleep_ms(u32 ms)
{
	rg_time_sleep_ns((u64)ms * UINT64_C(1000000));
}

RGINLINE void rg_time_yield(void)
{
	RG_TIME_PLATFORM_YIELD();
}

#endif // RG_TIME_H
