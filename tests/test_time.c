// rg_time public API correctness tests

#include "../src/rg_time.h"
#include "../src/rg_time.h"

#include <math.h>
#include <stdio.h>

static int tests_run;
static int tests_failed;

#define CHECK(condition)                                                \
	do                                                                  \
	{                                                                   \
		tests_run++;                                                    \
		if (!(condition))                                               \
		{                                                               \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
			tests_failed++;                                             \
		}                                                               \
	} while (0)

static int nearly_equal(f64 a, f64 b, f64 tolerance)
{
	return fabs(a - b) <= tolerance;
}

static void test_frequency_and_conversions(void)
{
	u64 frequency = rg_time_ticks_per_second();
	CHECK(frequency > 0u);
	CHECK(nearly_equal(rg_time_ticks_to_seconds(frequency), 1.0, 0.000001));
	CHECK(nearly_equal(rg_time_ticks_to_ms(frequency), 1000.0, 0.001));
	CHECK(nearly_equal(rg_time_ticks_to_us(frequency), 1000000.0, 1.0));
	CHECK(rg_time_seconds_to_ticks(1.0) == frequency);
	CHECK(rg_time_ms_to_ticks(1000.0) == frequency);
	CHECK(rg_time_us_to_ticks(1000000.0) == frequency);
	CHECK(rg_time_seconds_to_ticks(0.0) == 0u);
	CHECK(rg_time_ms_to_ticks(-1.0) == 0u);
	CHECK(rg_time_us_to_ticks(NAN) == 0u);
	CHECK(rg_time_seconds_to_ticks(HUGE_VAL) == UINT64_MAX);
	CHECK(rg_time_ticks_to_seconds(0u) == 0.0);
	CHECK(rg_time_ticks_to_ms(0u) == 0.0);
	CHECK(rg_time_ticks_to_us(0u) == 0.0);
}

static void test_monotonic_clock(void)
{
	u64 first = rg_time_ticks();
	u64 second = rg_time_ticks();
	CHECK(second >= first);

	rg_time_sleep_ns(0u);
	rg_time_sleep_us(0u);
	rg_time_sleep_ms(1u);
	u64 after_sleep = rg_time_ticks();
	CHECK(after_sleep > second);
	CHECK(rg_time_ticks_to_seconds(after_sleep - second) > 0.0);
	CHECK(rg_time_seconds() > 0.0);
	CHECK(rg_time_ms() > 0.0);
	CHECK(rg_time_us() > 0.0);

	rg_time_yield();
}

// Many back-to-back samples, with yields to invite migration between cores,
// must never step backwards. This matters most for the rdtsc backend, which
// relies on the invariant TSC being synchronized across cores.
static void test_monotonic_many_samples(void)
{
	u64 previous = rg_time_ticks();
	int backwards = 0;
	for (u32 i = 0; i < 2000000u; i++)
	{
		u64 now = rg_time_ticks();
		if (now < previous) backwards++;
		previous = now;
		if ((i & 0x3ffffu) == 0u) rg_time_yield();
	}
	CHECK(backwards == 0);
}

// The tick clock must agree with the OS clock about a 50 ms sleep. With the
// rdtsc backend this validates the calibrated frequency; with the OS backend
// both sides read the same clock.
static void test_elapsed_matches_os_clock(void)
{
#if !RG_TIME_CUSTOM
	u64 os_frequency = rg_time_os_frequency();
	CHECK(os_frequency > 0u);
	if (os_frequency == 0u) return;

	u64 os_start = rg_time_os_ticks();
	u64 tick_start = rg_time_ticks();
	rg_time_sleep_ms(50u);
	u64 tick_end = rg_time_ticks();
	u64 os_end = rg_time_os_ticks();

	f64 os_seconds = (f64)(os_end - os_start) / (f64)os_frequency;
	f64 tick_seconds = rg_time_ticks_to_seconds(tick_end - tick_start);
	CHECK(os_seconds >= 0.05);
	CHECK(tick_seconds >= 0.05);
	// The OS samples bracket the tick samples, so the OS span is slightly
	// longer. Allow 2% for that and for calibration error.
	CHECK(nearly_equal(tick_seconds, os_seconds, os_seconds * 0.02));
	printf("rg_time: backend=%s frequency=%llu Hz sleep(50ms) ticks=%.6fs os=%.6fs\n",
	       rg_time_using_tsc() ? "tsc" : "os", (unsigned long long)rg_time_ticks_per_second(),
	       tick_seconds, os_seconds);
#endif
}

int main(void)
{
	rg_time_init();
	rg_time_init();
	test_frequency_and_conversions();
	test_monotonic_clock();
	test_monotonic_many_samples();
	test_elapsed_matches_os_clock();
	printf("rg_time: %d checks, %d failures\n", tests_run, tests_failed);
	return tests_failed == 0 ? 0 : 1;
}
