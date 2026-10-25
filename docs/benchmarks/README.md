# Benchmark reports

The benchmark suites measure public `rg_core` headers against established C
and C++ libraries in the same optimized executables. Results are specific to
the published hardware, compiler, configuration, and workloads; they are not
performance guarantees.

C is the supported library language. The `.cpp` benchmark harnesses require a
C++17 compiler.

- [`rg_sprintf`](../rg_sprintf.md#performance) compares the portable and x64
  assembly formatters with `stb_sprintf` across integer, floating-point, string,
  and mixed game-style formats.
- [`rg_math`](../rg_math.md#performance) compares individual math operations
  with cglm and a weighted 3D-engine hot-path model across fully covered math
  libraries.
- [`rg_prof`](../rg_prof.md#performance) compares named event recording, frame
  bookkeeping, frame-history consumption, and cache-line-isolated worker state
  with fixed-section and packed-state baselines.
- [`rg_algo`, `rg_hash`, and `rg_containers`](rg_core.md) cover sorting,
  selection, integer-key hash maps, arrays, small vectors, rings, and sparse
  sets, including methodology and optional comparison-library versions.

The source harnesses are in [`benchmarks/`](../../benchmarks). The published
figures come from earlier runs; current sources, harnesses, and compilers may
produce different results.

## Current regression benchmarks

On Linux x86-64:

```sh
./build.sh bench_regression
./build.sh bench_math
./build.sh bench_sprintf
./build.sh bench_storage
./build.sh bench_input
./build.sh bench_prof
```

`./build.sh bench` runs the algorithm, hash, and container suites. Set
`RG_BENCH_BUILD_ONLY=1` to compile without running. Linux executables are written
under `.bench-build/linux/current`, for example:

```sh
RG_BENCH_BUILD_ONLY=1 ./build.sh bench_sprintf
.bench-build/linux/current/bench_sprintf --validate
RG_BENCH_BUILD_ONLY=1 ./build.sh bench_storage
.bench-build/linux/current/bench_storage =storage.rng.u32.constant10
```

The Linux builds use baseline x86-64 compiler settings. On an AVX2-capable host,
the math suite additionally builds and runs `bench_math_avx2` with `-mavx2` (and
`-mfma` where available) so the SSE4.1/AVX2/FMA code paths are measured, and the
formatter suite also builds and runs a separately compiled assembly backend;
otherwise it lists and runs only the C backend. `RG_BUILD_SIMD=off` omits the
assembly backend. SDL3 is discovered through `pkg-config` for the input suite.
`RG_BENCH_DEPS` enables locally available comparison headers; the scripts do not
download dependencies. The output sink is compiled separately without LTO.

The process-median and paired comparison reporting scripts below remain Windows
tools. Linux commands currently build and run the suites and preserve their raw
output format. Published Windows results should be treated as historical
measurements, rather than baselines for this machine.

From a Visual Studio Developer Command Prompt:

```bat
build.bat bench_regression
build.bat bench_math
build.bat bench_sprintf
build.bat bench_storage
build.bat bench_input
build.bat bench_prof
```

`bench_regression` builds and runs those five suites. SDL3 is required for input;
the other four suites run independently. Binaries and build metadata are written
under `.bench-build/current`. Each case emits seven raw `BENCH` samples in
nanoseconds per operation after one warmup. Pass a case-name substring directly
to an executable to select cases, for example:

```bat
.bench-build\current\bench_storage.exe storage.rng
```

The formatter harness covers bounded string slices, varied string lengths,
dynamic widths, affixed formats, direct integer and float conversion, and builder
appends. Integer cases cover 10–20 decimal digits and mixed signed/unsigned values.
Scanning cases cover 0–4096-byte strings with four input alignments, literal
formats, and formats containing `%s`. In `affixed_N`, N counts literal bytes;
the format also contains the two-byte conversion. Bounded scan cases vary string
precision independently of output capacity.
Storage includes string replacement with growth and with reused capacity; math
includes component-wise vec4 floor/sqrt, ray/box and ray/sphere hit and miss
cases, frustum culling (single, SoA, and batched), quaternion interpolation,
general and affine matrix inverses, and Perlin noise. The `*8` math cases
(`math.frustum_cull_aabbs8`, `math.frustum_cull_spheres8`, `math.ray_inv_aabbs8`)
process eight objects per operation; divide ns/op by 8 for the per-object cost.
The harnesses use rotating inputs and observable outputs. The math shared-profile
operations can be combined using the
[documented weights](../rg_math.md#performance).

The common sink is compiled separately without LTCG to prevent the compiler
discarding outputs. Output consumption, validation, and input preparation are
outside the measured inner loop. Profiler timestamps and pointer-containing
records make its checksums process-dependent; checksum equality is not a
cross-process correctness criterion. The profiler sum control can collapse to
closed-form arithmetic and is not subtracted from measured operation times.

Set `RG_BENCH_DEPS` to an external directory containing `stb_sprintf.h` (1.10)
to include the formatter comparison. Math dependency configuration and versions
are documented in the [math guide](../rg_math.md). No comparison dependency is
required by the library itself.

## Linux before/after, October 2026

The performance pass of October 2026 was measured on an Intel i3-1005G1 with
GCC 13.3 at the `build.sh` flags (`-O2 -DNDEBUG`, SSE2 baseline; AVX2 columns
from `bench_math_avx2`). Baseline and new binaries ran alternately on one pinned
core; values are medians of three full runs (two for the sort and hash tables)
in nanoseconds per operation unless stated. Differences under about 10%, and up
to 20% on sub-10 ns cases, are code-layout noise on this CPU.

| Sort and selection, 1,048,576 `int32` | before | after | `std::sort` |
| --- | ---: | ---: | ---: |
| random | 73.3 ms | **37.8 ms** | 72.5 ms |
| 256 distinct values | 39.4 ms | **11.2 ms** | 37.0 ms |
| nearly sorted | 11.5 ms | **8.1 ms** | 12.4 ms |
| sorted / reverse / all equal | 0.41 / 0.82 / 0.40 ms | unchanged | 10.2 / 7.1 / 7.9 ms |
| 32-byte records, 16-bit keys | 80.0 ms | **41.9 ms** | 77.0 ms |
| 32-byte records, 64-bit keys | 82.8 ms | **52.6 ms** (radix 65.8) | 82.2 ms |
| `nth_element`, random | 7.92 ms | **2.68 ms** | 5.27 ms |
| `nth_element`, 256 distinct values | 6.31 ms | **2.89 ms** | 8.62 ms |

| Math, SSE2 build | before | after | | Math, AVX2+FMA build | before | after |
| --- | ---: | ---: | --- | --- | ---: | ---: |
| `vec4_floor` | 6.81 | **1.49** | | `vec3_length` | 2.63 | **0.93** |
| `vec3_scale` | 1.07 | **0.65** | | `vec4_dot` | 2.44 | **0.99** |
| `vec4_length` | 1.61 | **1.01** | | `vec4_length` | 2.40 | **1.20** |
| `quat_mul` | 3.19 | **1.95** | | `vec4_normalize` | 2.42 | **1.49** |
| `vec3_cross` | 1.41 | **0.94** | | `vec3_normalize` | 2.56 | **1.75** |
| `mat4_mul4` | 26.1 | **17.5** | | `vec3_cross` | 1.37 | **0.99** |
| `ray_sphere_forward` | 10.6 | **6.9** | | `quat_normalize` | 1.04 | 1.51 |
| `vec4_normalize` | 1.38 | 1.93 | | `look_at_rh` | 8.0 | 10.0 |
| `look_at_rh` | 13.6 | 17.1 | | `frustum_extract_no` | 11.3 | 17.2 |

The normalization-chain rows (`*_normalize`, `look_*`, `frustum_extract_*`,
`mat4_decompose_fast`) are slower by design: the default `RG_MATH_RSQRT_MODE`
now refines the hardware estimate with one Newton-Raphson step, taking unit
lengths from 3e-4 to 3e-7 error. `RG_MATH_RSQRT_MODE_RSQRT_RAW` restores the
previous speed. New cases measured only after the change (SSE2 / AVX2):
`quat_slerp` 17.9 / 10.1 (previous implementation about 27 / 21 in the audit
harness), `ray_aabb_hit` 6.5 / 5.8 (about 23), `aabb_in_frustum_mixed` 5.5 / 2.7
(about 15), `frustum_cull_aabbs8` 36 / 20 per eight boxes, `perlin_vec3` 23 / 23
(about 173 / 79), `mat4_inv` 9.9 / 8.7 (about 11 / 12).

| Formatter, C backend | before | after | | Runtime | before | after |
| --- | ---: | ---: | --- | --- | ---: | ---: |
| `literal_32` | 76.6 | **20.3** | | `prof.timer` | 17.6 | **8.0** |
| `literal_4096` | 9189 | **108** | | `prof.scope_tls` | 36.4 | **16.2** |
| `mixed` | 183 | **151** | | `prof.frame` | 37.4 | **16.5** |
| `slice_scan_4096` | 903 | **48** | | `input.absolute` | 88.7 | **43.9** |
| `strlen_63` | 16.1 | **4.7** | | `input.absolute_single` | 71.0 | **29.9** |
| `affixed_1` | 20.7 | 28.4 | | `storage.rng.shuffle_u32` | 7.35 | **3.79** |

`%e` of 1.5e300 dropped from about 1370 ns to about 40 ns (the exponent loop was
linear in the exponent); it is not a harness case. One-character literal runs
(`affixed_1`, `literal_1`) cost a few nanoseconds more. String-key hash
insertion improved 30-50% (`storage.hash.*_string32.insert_reserved`); integer
keys, lookups, and removals are unchanged within noise.

## Before/after comparison

Use a checkout or exported source tree for the baseline. The runner builds the
same harness against that tree's headers and the current headers. Both trees
must provide the APIs used by the harness:

```powershell
tools/compare_bench.ps1 -BaselineRoot path/to/baseline -Runs 7
```

The runner compares each case in adjacent baseline/candidate processes and
alternates their order between pairs. It pins the benchmark thread to the same
available logical CPU and writes raw output, build fingerprints, paired process
medians, and a Markdown report to `.bench-build/comparison`. The collapsible
profiler sum control is excluded from the regression comparison.
`-Suites storage -Filter storage.rng` narrows a run; `-SkipBuild` reuses binaries.
`-OutputPath` selects another report directory for confirmation runs.
Use `-Cases case.one,case.two` to confirm a list of exact case names.

Use `-CandidateRoot` to build a frozen candidate snapshot instead of the current
headers. `-BaselineBuildPath` and `-CandidateBuildPath` select separate build
directories, preserving the executables from earlier experiments. Paths may be
absolute or relative to the repository. Source snapshot paths and input hashes
identify an uncommitted candidate; the recorded workspace revision alone does
not identify that snapshot.

The runner checks source and executable hashes before reusing a build. Each
benchmark accepts `--list` to enumerate cases and `=case.name` for an exact
match; ordinary arguments remain substring filters. Storage and formatter
checksums must match across every baseline and candidate sample.
Math results, live input, and profiler timestamps use the correctness suites
instead of cross-process checksum equality.

Rebuild after changing the compiler, build options, or external dependencies;
`-SkipBuild` does not validate those inputs.

Positive changes mean slower. `RECHECK` flags consistent losses or median
losses over 2% for investigation; it is not an acceptance threshold. Confirm
slowdowns in a separate paired run. Improvements in one case do not offset a
repeatable regression elsewhere. Run without concurrent builds or other
benchmark processes; retain CPU/compiler details and raw samples with results.

Timing also depends on executable layout and machine state. Use identical-binary
controls when investigating small differences, and measure representative game
workloads before treating a microbenchmark result as a general speedup.
A run that does not establish a difference does not prove equal performance.
