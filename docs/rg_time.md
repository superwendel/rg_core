# rg_time

`rg_time.h` provides high-resolution monotonic timestamps, duration
conversions, sleeping, and thread yielding. It depends only on
[`rg_defs.h`](rg_defs.md).

## Setup

Include the header wherever timing helpers are used:

```c
#include "rg_time.h"

int main(void)
{
  rg_time_init();
  // Start worker threads and run the application.
}
```

All functions have internal linkage. The header can be included directly in
unity builds or separately compiled C translation units.

Native backends are included for Windows, Linux, and macOS. Other platforms
can provide the four custom hooks described below.

For strict C99/C11 builds on Linux, expose the POSIX declarations before any
system headers are included. The native build script supplies
`-D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L`; use the same compiler flags in
your application. Defining feature macros after including `rg_defs.h` or other
system headers is too late. Run `./build.sh test_time` to check both the native
and custom backends.

Call `rg_time_init` once before starting worker threads. It caches the invariant
platform frequency so subsequent conversions are arithmetic-only. Because the
header has internal linkage, separately compiled translation units that need
the cached path must each call `rg_time_init`; a unity build has one cache.
Calls made without initialization remain correct but query the platform
frequency each time. On x86-64, `rg_time_init` also selects and calibrates the
clock backend described below; make it the first `rg_time` call.

## TSC backend

On x86-64 (`RG_TIME_USE_TSC` defaults to `1`) the timestamp is the processor's
time stamp counter read with `rdtsc`. An OS clock read costs about 18 ns
(`clock_gettime`) or 20 ns (`QueryPerformanceCounter`); `rdtsc` costs about
8 ns, which halves the cost of an `rg_prof` scope. The backend is chosen at
run time and falls back to the OS clock whenever the hardware does not qualify:

- CPUID leaf `0x80000007` must report an invariant TSC (EDX bit 8), meaning
  the counter runs at a constant rate across frequency and sleep states.
  Processors since roughly 2008 and most hypervisors report it; without it the
  OS clock is used.
- The TSC frequency comes from CPUID leaf `0x15` (crystal frequency times the
  TSC/crystal ratio) when the processor reports it. Intel cores since Skylake
  do, and this is the same value the Linux kernel uses for its `tsc`
  clocksource, so the two clocks agree exactly. The probe costs about a
  microsecond.
- Otherwise (AMD, older Intel, many virtual machines) the frequency is measured
  against the OS clock over `RG_TIME_TSC_CALIBRATE_NS`, which defaults to
  10 ms. Each end of the window takes the tightest of sixteen `rdtsc` /
  OS-clock sample brackets, so preemption and cache misses cannot skew the
  estimate; the remaining uncertainty is a few nanoseconds per end, or about
  1 ppm over 10 ms. Measurements on an Ice Lake laptop with the probe forced to
  calibrate were within 1 ppm of the leaf `0x15` value at both 10 ms and 2 ms.
  The window is slept through, not spun, and it is paid once by the first
  `rg_time` call, which should be `rg_time_init` on the main thread before
  worker threads start. Raise the window on hosts where `rdtsc` is trapped by
  a hypervisor, or lower it when start-up latency matters more than the last
  few ppm.

`rg_time_using_tsc()` reports which backend is active, and
`rg_time_ticks_per_second()` returns the TSC frequency when it is, so code that
converts with the `rg_time` helpers needs no changes. Ticks are no longer
nanoseconds, however; code that assumed a 1 GHz tick must convert.

Accuracy and caveats:

- Relative to the OS clock, durations drift by at most the calibration error:
  zero when leaf `0x15` is used, about 1 ppm (and bounded well under 20 ppm)
  when measured. Relative to true time both clocks carry the crystal's
  tolerance, typically tens of ppm, since neither is NTP-disciplined. Over a
  ten-minute session 20 ppm is 12 ms.
- Monotonicity relies on the TSCs of all cores being synchronized, which the
  invariant-TSC guarantee covers within one socket and which operating systems
  verify at boot. On multi-socket machines the counters of different sockets
  may be offset by a small, constant amount, so a thread migrating between
  sockets can observe a step of that size. Pin latency-sensitive threads, or
  build with `-DRG_TIME_USE_TSC=0` to force the OS clock.
- `rdtsc` is not a serializing instruction; neighbouring instructions can
  execute on either side of it. Sub-ten-nanosecond scopes may be attributed a
  few cycles early or late, which is the same trade-off every rdtsc-based
  profiler makes.
- A virtual machine that is live-migrated to a host with a different TSC rate
  invalidates the calibration until the process restarts.
- Ticks taken before the backend is selected come from the OS clock and are not
  comparable with ticks taken afterwards. Calling `rg_time_init` first avoids
  the mix.

`RG_TIME_USE_TSC` must be `0` or `1`. It may only be `1` on x86 with GCC, Clang,
or MSVC, and cannot be combined with `RG_TIME_CUSTOM`; other values or
combinations are compile errors. `RG_TIME_TSC_CALIBRATE_NS` must be positive.
On non-x86 targets and under `RG_TIME_USE_TSC 0` the OS clock paths are
unchanged: `clock_gettime(CLOCK_MONOTONIC_RAW)`, `QueryPerformanceCounter`, and
`mach_absolute_time`. The test suite covers both backends; build
`tests/test_time.c` with `-DRG_TIME_USE_TSC=0` for the forced fallback and with
`-DRG_TIME_TSC_FORCE_CALIBRATION` to exercise the OS-clock calibration on
hardware that reports leaf `0x15`.

## Measuring time

Use raw ticks when measuring a duration, then convert the difference:

```c
u64 start = rg_time_ticks();

update_world();

u64 elapsed_ticks = rg_time_ticks() - start;
f64 elapsed_ms = rg_time_ticks_to_ms(elapsed_ticks);
```

`rg_time_ticks_per_second` returns the platform frequency. Conversion helpers
are available for seconds, milliseconds, and microseconds in both directions.
If a backend reports a zero frequency, conversions return zero.

Floating-point durations that are negative, zero, or NaN convert to zero
ticks. Values beyond the `u64` tick range saturate to `UINT64_MAX`.

The convenience functions `rg_time_seconds`, `rg_time_ms`, and `rg_time_us`
return the current monotonic time in the requested unit. These are elapsed-time
values with an unspecified origin, not civil or Unix timestamps.

## Sleeping and yielding

```c
rg_time_sleep_ns(500000);
rg_time_sleep_us(500);
rg_time_sleep_ms(1);
rg_time_yield();
```

Sleep durations are requests to the operating system and may complete later
because of scheduler granularity. The Windows backend rounds nonzero
nanosecond requests up to the next millisecond. Linux and macOS retry an
interrupted `nanosleep` with its remaining duration.

Microsecond-to-nanosecond conversion saturates rather than wrapping. A zero
duration returns without calling the backend.

## Custom backend

Define `RG_TIME_CUSTOM` as `1`, provide functions matching the native hook
signatures, and name them before including the header:

```c
#include "rg_defs.h"

static u64 platform_ticks(void);
static u64 platform_frequency(void);
static void platform_sleep_ns(u64 ns);
static void platform_yield(void);

#define RG_TIME_CUSTOM 1
#define RG_TIME_PLATFORM_TICKS platform_ticks
#define RG_TIME_PLATFORM_FREQUENCY platform_frequency
#define RG_TIME_PLATFORM_SLEEP_NS platform_sleep_ns
#define RG_TIME_PLATFORM_YIELD platform_yield
#include "rg_time.h"
```

The frequency is the number of custom ticks per second. Custom timestamp and
frequency functions must return `u64`; sleep accepts nanoseconds, and yield
takes no arguments.

## Thread safety

The native implementation has one translation-unit-local frequency cache.
Timing, sleep, and yield functions can be called concurrently after
`rg_time_init` completes. The cache is read-only after initialization; do not
initialize it concurrently with timing or conversion calls. Calls that never initialize
the cache remain thread-safe and query the native backend each time. The TSC
backend selects itself exactly once: the first caller claims the probe with an
atomic compare-and-swap and publishes the result with a release store, and any
thread that arrives during the probe waits for that result, so concurrent first
use without `rg_time_init` still yields one backend and one frequency. A custom
backend is responsible for providing equivalent synchronization guarantees.
