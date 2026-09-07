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
dynamic widths, and affixed formats. The harnesses use rotating inputs and
observable outputs. The math shared-profile operations can be combined using the
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
