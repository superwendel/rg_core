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

Use the portable implementation directly when no assembly object is desired:

```c
#include "rg_sprintf.h"
```

Define configuration macros before inclusion when needed:

```c
#define RG_SPRINTF_NO_SIMD  // Disable SIMD hexadecimal encoding
#define RG_SPRINTF_NO_ASM   // Force the hybrid header to select portable C
#define RG_SPRINTF_SECURE   // Enable argument checks and parser limits
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

Pass an explicit precision from 0 through 17 to `rg_ftoa` and `rg_dtoa`. The
zero-value conversion path currently bypasses precision clamping.

## String builder

`RgBuilder` provides bounded append operations for strings, characters,
integers, floats, hexadecimal data, and formatted text. Appends truncate to the
provided capacity while preserving null termination when capacity is nonzero.

Use a positive-capacity buffer for `rg_builder_appendf`; a zero-capacity formatted
append currently underflows the stored length. Pass precision from 0 through 17
to `rg_builder_append_float` to avoid overflowing its temporary buffer when
formatting zero.

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

Enforcing string-precision bounds can affect code generation and placement
elsewhere in the formatter. Measure representative formats, string lengths,
and destination capacities with your build settings.

## Optimization approach

`rg_sprintf` optimizes integer, floating-point, and string conversion, and
recognizes selected complete formats such as `%s`, `%d`, and, in the ASM
header, `%.6f` and `%08u`. Other formats use the general parser. The library
does not recognize complete application messages. Benchmark simple complete
formats, mixed messages, and bounded output separately when choosing a backend.
