# rg_sprintf

`rg_sprintf.h` is a single-header C formatter intended for common game and
realtime workloads. It is not a promise of complete libc `printf`
compatibility for every locale or formatting edge case.

## Integration

For automatic backend selection, include the hybrid header:

```c
#include "rg_sprintf_hybrid.h"
```

On MSVC x64 AVX2 builds, the hybrid header selects `rg_sprintf_asm.h`. Compile
and link `src/asm/sprintf/win_x64/rg_sprintf_asm_x64.asm` in that configuration.
The Linux x64 helper also requires an AVX2-capable CPU. Compile
`src/asm/sprintf/linux_x64/rg_sprintf_asm_x64.S` and define `RG_SPRINTF_HAS_ASM`
to enable it.

For example, from the repository root, build a C application that includes the
hybrid header with GCC or Clang:

```sh
cc -std=c11 -O2 -mavx2 -DRG_SPRINTF_HAS_ASM \
   -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Isrc \
   example.c src/asm/sprintf/linux_x64/rg_sprintf_asm_x64.S \
   -lm -o example
```

The helper uses the Linux System V ABI and ELF object format. Use the MASM
source for Windows. `-mavx2` alone does not select assembly on Linux, and
`RG_SPRINTF_HAS_ASM` must only be defined when its helper object is linked.
There is no runtime CPU dispatch in the library: binaries that use this helper
require an AVX2-capable CPU. `./build.sh test_sprintf` checks portable and
fallback configurations, plus assembly and its ABI regression test when the
host supports AVX2.

Use the portable implementation directly when no assembly object is desired:

```c
#include "rg_sprintf.h"
```

Define configuration macros before inclusion when needed:

```c
#define RG_SPRINTF_NO_SIMD  // Disable SIMD hexadecimal encoding (SSSE3 or NEON)
#define RG_SPRINTF_NO_ASM   // Force the hybrid header to select portable C
#define RG_SPRINTF_SECURE   // Enable argument checks and parser limits
#define RG_SPRINTF_ROUND_TIES_AWAY 1 // Round exact float ties away from zero (legacy MSVC CRT style)
#define RG_SPRINTF_ASSERT(x) custom_assert(x)
```

The first formatter implementation included owns the translation unit. Later
portable, ASM, or hybrid includes retain that choice. For example, including
`rg_sprintf.h` before `rg_log.h` keeps portable C; including the logger first
keeps whichever backend its hybrid include selected. This also applies to the
formatter included by `rg_math_io.h`.

Set configuration before the first formatter-related include. When no backend
has been included, the hybrid header selects portable C if
`RG_SPRINTF_NO_ASM` or `RG_SPRINTF_HYBRID_FORCE_C` is defined; otherwise it
selects the ASM header if `RG_SPRINTF_HYBRID_FORCE_ASM` or
`RG_SPRINTF_HAS_ASM` is defined, then falls back to platform detection.
These controls do not replace an implementation already included directly.

`RG_SPRINTF_HYBRID_FORCE_ASM` selects the ASM header. External helpers are
enabled automatically on MSVC x64 AVX2 or explicitly by `RG_SPRINTF_HAS_ASM`,
and must be linked when enabled. Directly including `rg_sprintf_asm.h` with
`RG_SPRINTF_NO_ASM` retains that header's C fallback.

## Formatting API

```c
int rg_sprintf(char* buf, const char* fmt, ...);
int rg_snprintf(char* buf, size_t count, const char* fmt, ...);
int rg_vsprintf(char* buf, const char* fmt, va_list args);
int rg_vsnprintf(char* buf, size_t count, const char* fmt, va_list args);
```

`rg_snprintf` and `rg_vsnprintf` return the number of characters that would
have been written, excluding the terminator.

String precision bounds source reads: `%.*s` with a nonnegative precision
accepts a byte slice without a null terminator and reads at most that many
bytes, stopping earlier at a null. Zero precision does not read the source.
A negative dynamic precision behaves like an omitted precision and requires a
null-terminated string. Width adds padding independently of this bound.

Callback output is available through `rg_sprintf_cb` and `rg_vsprintf_cb`.
Callbacks receive temporary chunks and must consume or copy them before
returning.

## Direct conversion API

- `rg_itoa`, `rg_utoa`, `rg_i64toa`, and `rg_u64toa`
- `rg_ftoa` and `rg_dtoa`
- `rg_to_hex` and `rg_from_hex`

The numeric-to-string functions return a pointer to the terminating null byte.

`rg_dtoa(value, buf, precision)` writes `value` in `%f` style with `precision`
decimal places; a negative precision selects six. `rg_ftoa` converts the float
to double and does the same. The output is correctly rounded (see
[Floating-point output](#floating-point-output)), so `rg_dtoa(x, buf, p)`
produces exactly the text of `printf("%.*f", p, x)`. The buffer must hold
`RG_SPRINTF_FLOAT_BUFFER_SIZE` bytes to cover every double, or `24 + precision`
bytes when `|value| < 1e18`. Precision above `RG_SPRINTF_FLOAT_MAX_PRECISION`
(350) is clamped.

## Floating-point output

`%f`, `%e`, `%g`, `rg_dtoa`, `rg_ftoa`, and `rg_builder_append_float` derive
every digit from the exact binary value of the double with integer arithmetic.
Values a game prints use 128-bit multiplies; exponents beyond that range fall
back to a small bignum, so there is no precision cliff and no notation switch:
`%f` of `1e22` prints all 23 integer digits like libc does.

Rounding is round-half-to-even on the exact value, the IEEE 754 default that
glibc, musl, macOS, the BSDs, and the Universal CRT (Visual Studio 2015 and
later) use, so `%.2f` of `0.125` is `0.12`,
`%.0f` of `2.5` is `2`, and `%.0f` of `0.49999997` is `0`. The output matches
those libcs bit for bit for `%f`, `%e`, `%g` and their uppercase forms at
every precision up to 350, including the `+`, space, `#`, `0`, and `-` flags
and widths, with one exception: for `%#g` glibc drops the trailing zeros when
rounding carries into a new exponent (`%#.3G` of `999.866` prints `1.E+03`),
where rg_sprintf keeps them as C11 specifies (`1.00E+03`), as musl does.
The legacy Microsoft CRT, before Visual Studio 2015 or when a program links
`legacy_stdio_float_rounding.obj`, rounds exact ties away from zero (`%.0f` of
`2.5` is `3`); define `RG_SPRINTF_ROUND_TIES_AWAY` as `1` before the include to
match it. Either way the output is identical on every platform and compiler.

`%g` chooses between the `%e` and `%f` styles from the exponent after rounding
to the requested significant digits, so `%.1g` of `9.5` is `1e+01`. Infinity
and NaN print as `inf` and `nan`, uppercase for `%E`, `%F` and `%G`, and are
never zero padded.

## String builder

`RgBuilder` provides bounded append operations for strings, characters,
integers, floats, hexadecimal data, and formatted text. Appends truncate to the
provided capacity while preserving null termination when capacity is nonzero.

A builder with zero capacity, or one with only the terminator's byte left,
ignores appends and keeps its length. `rg_builder_append_float` accepts any
precision and truncates long results to the capacity.

## Performance

![rg_sprintf benchmark results](benchmarks/rg_sprintf-vs-stb.svg)

The historical figure compares [`stb_sprintf` 1.10](https://github.com/nothings/stb/blob/master/stb_sprintf.h),
the portable `rg_sprintf` header, and `rg_sprintf_asm.h` linked with the MASM
x64 helper. It reports the median of seven process runs. Each process was
pinned to one logical CPU and raised to high priority, warmed each
implementation with 1,000,000 calls per case, and then took the median of
three 10,000,000-call samples.

All three implementations were built into the same binary with MSVC
19.44.35217 for x64 using `/O2 /Ob3 /Oi /Ot /Oy /GL /LTCG /arch:AVX2
/fp:fast /GS- /DNDEBUG`. Measurements were taken on an AMD Ryzen 9 4900HS on
Windows build 26200.9168.

The cases cover `%d`, `%08u`, `%x`, `%lld`, `%.6f`, `%.6e`, `%g`, `%s`, and a
mixed game-style status string containing a name, two integers, and two
floating-point values. Values are nanoseconds per call, so lower is better.
The formatter includes exact-format fast paths for several of these cases;
these numbers therefore include dispatch optimizations. Results are
machine-specific and should not be treated as a performance guarantee.

### Running the checked-in harness

```bat
build.bat bench_sprintf
.bench-build\current\bench_sprintf.exe slice
.bench-build\current\bench_sprintf.exe --validate
```

The harness in `benchmarks/bench_sprintf.c` builds the portable and ASM
backends in separate translation units in one executable. Set `RG_BENCH_DEPS`
to a directory containing `stb_sprintf.h` to include the optional stb
comparison; the historical comparison used version 1.10. Source include paths
select the public headers, including when comparing against an older snapshot.

The current harness retains the published conversion categories and mixed
status format, and adds width-only strings, bounded output, short slices of
long strings, varied lengths, dynamic positive and negative widths, and affixed
formats. It also covers mixed signed and unsigned 64-bit formatting, direct
integer conversion across 10–20 decimal digits, direct float conversion, and
builder appends. Direct conversion and builder cases apply to the rg backends.
It uses rotating prebuilt inputs. Each case receives one warmup
and seven samples of 262,144 calls. Output buffers and returned lengths are
checked against libc and consumed outside each timed block. `--validate` checks
one batch per case without timing. `BENCH` rows report the case, sample index,
nanoseconds per call, and checksum; an optional first argument filters case
names by substring. This harness uses a different sampling and output-consumption
method from the historical figure, so its measurements should be reported as
new results rather than treated as a recreation of those exact timings.

### Linux x86-64 results

Measured with `./build.sh bench_sprintf` on an Intel Core i3-1005G1 laptop
(Ubuntu 24.04, GCC 13.3, `-O2 -DNDEBUG`; the assembly backend is the GNU
assembler build of the same x64 helper). Nanoseconds per call, median of seven
samples and three process runs on one pinned core. `stb_sprintf` was not
present on this machine. The float, `rg_dtoa`, builder, and status-line rows
were re-measured after the exact floating-point conversion landed.

| Case | Portable C | x64 assembly | | Case | Portable C | x64 assembly |
| --- | ---: | ---: | --- | --- | ---: | ---: |
| `%d` | 15.0 | 12.6 | | `%s` | 13.2 | 13.8 |
| `%08u` | 38.6 | 16.4 | | `%-20s` | 13.9 | 18.1 |
| `%x` | 16.3 | 11.1 | | `%.*s` slice | 17.1 | 19.7 |
| `%lld` | 39.7 | 17.1 | | bounded `%s` | 30.9 | 25.2 |
| `%.6f` | 26.2 | 19.8 | | mixed status line | 125 | 90 |
| `%.6e` | 32.2 | 28.5 | | 32-byte literal | 19.8 | 14.2 |
| `%g` | 41.1 | 35.9 | | 4,096-byte literal | 105 | 126 |
| `rg_u64toa`, 20 digits | 9.8 | 7.2 | | `%.*s` over 4,096 bytes | 46 | 48 |
| `rg_dtoa` | 8.8 | 8.8 | | `rg_strlen`, 128 bytes | 5.2 | 3.8 |
| builder `appendf` | 72.9 | 47.9 | | `rg_strlen`, 4,096 bytes | 38 | 53 |

Literal text, bounded string scans, and `rg_strlen` cost about the same in
both backends because the portable header copies literal runs in bulk and uses
`memchr`/`strlen`; the assembly backend's advantage is concentrated in integer
conversion. The exact floating-point conversion made the portable backend's
`%.6f`, `%.6e`, `%g` and `rg_dtoa` 28 to 37 percent faster than the previous
double-arithmetic code at these inputs. Under GCC the assembly backend's `%.6f`
and `%.6e` cost about 3 ns more than that older code did, since it had used an
inexact fast path for values below 1e18; under Clang the exact code is faster
than it on every case. `%e` of `1e300` or `5e-324` costs about the same as
`%e` of `1.5`.

Enforcing string-precision bounds can affect code generation and placement
elsewhere in the formatter. Measure representative formats, string lengths,
and destination capacities with your build settings.

## Optimization approach

`rg_sprintf` optimizes integer, floating-point, and string conversion, and
recognizes selected complete formats such as `%s`, `%d`, and, in the ASM
header, `%.6f` and `%08u`. Other formats use the general parser. The library
does not recognize complete application messages. Benchmark simple complete
formats, mixed messages, and bounded output separately when choosing a backend.

In both headers, literal text between conversions is located with `strchr`
and copied with one bulk write, so long literal runs cost about the same as a
`memcpy`. Floating-point digits come from the exact value: for `%f` the
fraction bits are multiplied by `5^precision` in 128 bits and shifted, with the
discarded bits deciding the rounding; for `%e` and `%g` the decimal exponent is
estimated from the binary exponent and any surplus leading digits are divided
away exactly, so cost does not grow with the magnitude of the exponent. Both
backends share this code, so they produce identical digits. String precision
(`%.*s`) is bounded with `memchr`, and the portable string length helper checks
eight bytes inline before deferring to the platform `strlen` (the ASM backend
uses its assembly helper).
