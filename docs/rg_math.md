# rg_math

`rg_math` is a header-only C99 math library for game and realtime code. It
provides scalar helpers, vectors, matrices, quaternions, camera transforms,
curves, geometry queries, debug printing, and Perlin noise.

## Usage

Include the umbrella header for the complete library:

```c
#include "rg_math.h"

rg_vec3 position;
rg_vec3 velocity;
rg_vec3 next;

rg_vec3_set(&position, 1.0f, 2.0f, 3.0f);
rg_vec3_set(&velocity, 4.0f, 0.0f, -1.0f);
rg_vec3_add(&position, &velocity, &next);
```

All functions have internal linkage, so there is no implementation macro or
separately compiled `rg_math` library. Debug printing uses the formatter backend
described below. The headers use the `f32`, `f64`, and fixed-width integer
aliases from `rg_defs.h`.

## Modules

The umbrella always includes the scalar, vector, and matrix base. The remaining
modules can also be included directly:

- `rg_math_scalar.h` — scalar math, conversions, interpolation, and approximations.
- `rg_math_vec.h` — floating-point and integer vectors.
- `rg_math_mat.h` — column-major matrices, transforms, projection, and decomposition.
- `rg_math_cam.h` — camera, projection, and frustum helpers.
- `rg_math_quat.h` — quaternion operations and conversions.
- `rg_math_euler.h` — ordered Euler-angle conversions.
- `rg_math_color.h` — linear RGB calculations.
- `rg_math_curve.h` — Bezier, spline, and easing helpers.
- `rg_math_geom.h` — geometric primitives, bounds, and intersections.
- `rg_math_io.h` — configurable debug printing.
- `rg_math_noise.h` — classic Perlin gradient noise in 2D, 3D, and 4D.

To trim the umbrella, define any of these before including `rg_math.h`:

```c
#define RG_MATH_NO_CAM
#define RG_MATH_NO_QUAT
#define RG_MATH_NO_EULER
#define RG_MATH_NO_COLOR
#define RG_MATH_NO_CURVE
#define RG_MATH_NO_GEOM
#define RG_MATH_NO_IO
#define RG_MATH_NO_NOISE
#include "rg_math.h"
```

`RG_MATH_NO_QUAT` also omits Euler helpers, which depend on quaternions.
`RG_MATH_NO_GEOM` also omits IO helpers, whose public API includes geometry
types. These switches affect only the umbrella header; direct module includes
still include their required dependencies.

## SIMD and portability

On x86 and x64, compiler-enabled SSE, SSE4.1, FMA, and AVX paths are selected at
compile time. The library does not perform runtime CPU dispatch, so the emitted
instructions follow the compiler target. For example, `/arch:AVX2` enables the
corresponding optimized paths in an MSVC build and requires an AVX2-capable
machine at runtime.

Other architectures use the scalar implementation. Define
`RG_MATH_NO_SIMD` to force that implementation on any target:

```c
#define RG_MATH_NO_SIMD
#include "rg_math.h"
```

`rg_vec3`, `rg_vec4`, `rg_quat`, and `rg_mat4` are 16-byte aligned.
`rg_mat4_mul4_aligned32` requires every input and the output to be **32-byte
aligned**. Use `rg_mat4_mul4` with ordinary `rg_mat4` storage:

```c
RG_ALIGN32 rg_mat4 chain[4];
RG_ALIGN32 rg_mat4 product;
/* Initialize all four matrices before multiplying. */
rg_mat4_mul4_aligned32(&chain[0], &chain[1], &chain[2], &chain[3], &product);
```

When allocating this storage dynamically, request 32-byte alignment from the
allocator. Aligning a pointer variable does not align the memory it points to.
Both four-matrix multiplication functions allow the output to alias any input.

## Performance and safety modes

`RG_MATH_MAX_PERF` defaults to `1`. This selects the library's fastest paths,
including hardware reciprocal square root for normalization (refined by one
Newton-Raphson step, see `RG_MATH_RSQRT_MODE` below), and removes some
zero-length and validity checks. In this mode:

- Pass normalized axes, normals, and quaternions to APIs that document that requirement.
- Initialize `rg_vec3` values with `rg_vec3_set` or explicitly initialize `_pad`
  before relying on SIMD loads.
- Treat `*_fast` functions as unchecked operations with documented preconditions.
- Do not use `rg_vec*_isnan`, `rg_vec*_isinf`, or `rg_vec*_isvalid` as validation;
  max-performance builds intentionally avoid those checks.

For checked normalization and validity paths, configure the library before its
first include:

```c
#define RG_MATH_MAX_PERF 0
#include "rg_math.h"
```

The setting is translation-unit-wide and must be consistent wherever math
types and inline functions are used.

Additional configuration:

- `RG_MATH_ASSERT(expression)` replaces the assertion hook.
- `RG_MATH_USE_LIBC` controls use of `<math.h>` fallbacks.
- `RG_MATH_LIBC_ALIASES` allows direct macro wrappers when libc is enabled.
- `RG_MATH_FAST_MATH`, `RG_MATH_FAST_TRIG`, and `RG_MATH_FAST_EXP` opt into
  lower-precision approximations for the corresponding scalar wrappers.
- `RG_MATH_RSQRT_MODE` selects how `rg_rsqrtf`, `rg_rsqrtf_fast`, and every
  SIMD normalization compute the reciprocal square root in max-performance
  builds. `RG_MATH_RSQRT_MODE_RSQRT` (default) uses the hardware estimate plus
  one Newton-Raphson step (relative error about 2.4e-7, so normalized vectors and
  quaternions have unit length to within 3e-7). `RG_MATH_RSQRT_MODE_SQRT_RCP`
  uses `rcp(sqrt(x))`. `RG_MATH_RSQRT_MODE_RSQRT_RAW` uses the unrefined hardware
  estimate (relative error about 3e-4); it saves roughly 1 ns per normalization
  and 10 ns in chains such as `look_at` and frustum extraction, at the cost of
  visible drift when renormalizing repeatedly.
- `RG_MATH_SINCOS_POLY` forces the max-performance `rg_sincosf` polynomial. By
  default, baseline (non-FMA) builds on glibc call `sincosf` instead because it
  is faster there; FMA builds and other libcs always use the polynomial.
- `RG_MATH_VEC3_PLAIN` and `RG_MATH_VEC4_PLAIN` omit the `__m128` union member
  while retaining an aligned four-component array layout.

Boolean numeric options accept only `0` or `1`; invalid configuration values
produce a compile-time error.

## Coordinate conventions

Matrices are column-major and multiply column vectors: `out = matrix * vector`.
In `a * b`, the transform `b` acts first. Quaternions store `(x, y, z, w)` and
rotation angles are radians. View matrices map the camera eye to the origin
and forward to negative Z for right-handed cameras or positive Z for
left-handed cameras. Projection helpers default to right-handed clip
space with an NDC depth range of `[-1, 1]`:

```c
#define RG_MATH_CLIP_CONTROL RG_MATH_CLIP_CONTROL_RH_NO
#include "rg_math.h"
```

The other supported values are `RG_MATH_CLIP_CONTROL_RH_ZO`,
`RG_MATH_CLIP_CONTROL_LH_NO`, and `RG_MATH_CLIP_CONTROL_LH_ZO`. Explicit
`_rh_no`, `_rh_zo`, `_lh_no`, and `_lh_zo` variants are available when a call
site should not depend on the translation-unit default.

```c
rg_vec3 eye = rg_vec3(0.0f, 2.0f, 5.0f);
rg_vec3 target = rg_vec3(0.0f, 0.0f, 0.0f);
rg_vec3 up = rg_vec3(0.0f, 1.0f, 0.0f);
rg_mat4 view;
rg_mat4 projection;

rg_mat4_look_at(&eye, &target, &up, &view);
rg_mat4_perspective(RG_PI / 3.0f, 16.0f / 9.0f, 0.1f, 1000.0f,
                    &projection);
```

`rg_frustum_from_mat4` uses the configured depth range when extracting planes
from a view-projection matrix. Use `rg_frustum_from_mat4_no` for `[-1, 1]` or
`rg_frustum_from_mat4_zo` for `[0, 1]` when the projection was built with an
explicit variant. These plane extractors support either handedness.

## Transform preconditions

- `rg_mat4_inv_tr` inverts orthonormal rotation plus translation. Its last row
  must be `[0, 0, 0, 1]`; scale and shear require `rg_mat4_inv_affine` instead.
  Both inverse functions support `out == input`.
- Matrix-to-quaternion conversion expects an orthonormal rotation matrix in
  max-performance mode. Use `rg_mat4_decompose` or `rg_mat4_decompose_fast` for
  a translation/rotation/scale matrix with nonzero scales and no shear. A
  decomposition may choose different signed scales and an equivalent rotation;
  recomposition preserves the original transform.
- Quaternion rotation/interpolation and quaternion-to-matrix conversion expect
  unit quaternions in max-performance mode. Axis-angle construction expects a
  unit axis. `rg_quat_from_vecs` expects two unit vectors;
  `rg_quat_from_norm_pair_fast` additionally excludes antiparallel vectors.

## Debug printing

`rg_math_io.h` supplies print helpers for math and geometry types. Printing is
enabled in debug builds and compiles to no-ops when `NDEBUG` is defined. Define
`RG_MATH_DEFINE_PRINTS` to enable it in a release build or
`RG_MATH_PRINT_NO_COLOR` to disable ANSI color sequences.

When printing is enabled, `rg_math_io.h` uses `rg_sprintf_hybrid.h`. Define
`RG_MATH_IO_NO_SPRINTF_INCLUDE` only when a compatible `rg_sprintf` API was
already included earlier in the translation unit.

On MSVC x64 AVX2 builds, the hybrid formatter selects its ASM backend by
default. Link the helper described in [formatter integration](rg_sprintf.md#integration),
or define `RG_SPRINTF_NO_ASM` before the first formatter-related include to use
portable C for debug printing.

### Culling and ray queries

`rg_aabb_in_frustum` and `rg_sphere_in_frustum` are branch-free. The box test
evaluates `n·p + d` per plane, where `p` is the corner selected by the signs of
the normal, so it is exact even for boxes whose extent is below float
resolution at their coordinates. For many objects per frustum convert the planes
once with `rg_frustum_soa_from_frustum` into an `rg_frustum_soa` (32-byte
aligned, six planes padded to eight lanes) and use:

- `rg_aabb_in_frustum_soa(aabb, soa)` / `rg_sphere_in_frustum_soa(sphere, soa)`
  give the same results as the `rg_frustum` variants.
- `size_t rg_frustum_cull_aabbs(soa, boxes, count, out)` and
  `size_t rg_frustum_cull_spheres(soa, spheres, count, out)` write one byte
  per object (1 = intersecting or inside) and return the visible count. Any
  `count` is allowed, including 0. AVX builds test eight boxes at a time; every
  path gives identical flags for the same build.

`rg_ray_aabb` is a branchless slab test. Direction components may be zero
(including `-0.0`); a ray lying exactly in a face plane counts as touching the
box. `tmin`/`tmax` are written whenever the slab intervals overlap, which
includes a box entirely behind the origin (the call then returns 0); they are
left untouched when the slabs do not overlap. For many boxes per ray
precompute the reciprocal direction:

```c
rg_ray_inv inv;
rg_ray_inv_make(&inv, &ray);
int hit = rg_ray_inv_aabb(&inv, &box, &tmin, &tmax);              // tmin/tmax always written
size_t hits = rg_ray_inv_aabbs(&inv, boxes, count, tmins, flags); // tmins may be NULL
```

`rg_ray_triangle` defers its division until after the barycentric rejects;
hits, misses, and `t` are unchanged within rounding.

### Accuracy notes

- `rg_quat_slerp` (max-performance mode) uses a polynomial arccosine accurate
  to 2e-8 rad and evaluates `sin`/`cos` of `t * angle` without range reduction
  for `0 <= t <= 1`; results match a double-precision slerp to about 5e-7 per
  component. Values of `t` outside `[0, 1]` still work through the
  range-reduced path.
- Quaternion and vector normalization use the `RG_MATH_RSQRT_MODE` reciprocal
  square root; in the default mode the length error is about 3e-7 instead of
  the 3e-4 of the raw hardware estimate.
- `rg_frustum_from_mat4*` initialize `normal._pad` of every plane to 0.
- `rg_vec4_floor` and `rg_vec3_floor` match `floorf` bit for bit, including the
  sign of zero, in every build.

## Noise

`rg_perlin_vec2`, `rg_perlin_vec3`, and `rg_perlin_vec4` implement Ken Perlin's
improved (2002) gradient noise: integer lattice hashing through the reference
256-entry permutation table (`p[p[p[X] + Y] + Z]`), a quintic fade, and
branch-free gradient selection from fixed edge-direction tables (8 directions in
2D, the 12 cube edges in 3D, the 32 tesseract edges in 4D). The lattice repeats
every 256 units on each axis.

Guarantees:

- Output is always within `[-1, 1]` for every finite input. The raw noise is
  scaled by `0.9999`, `0.9649`, and `0.6507` (2D, 3D, 4D), which are the
  reciprocals of each gradient set's provable peak, so no permutation or
  rounding can exceed the range. The realised extremes with the built-in
  permutation are about ±1.0 (2D), ±1.0 (3D), and ±0.77 (4D); standard
  deviations over uniform samples are about 0.27, 0.26, and 0.19.
- Integer lattice points evaluate to exactly `0`.
- Values are deterministic and C2-continuous (the fade has zero first and
  second derivatives at cell boundaries).
- Coordinates are evaluated in `f32`. A coordinate whose magnitude exceeds
  2^24 is already integral, so the noise degenerates along that axis; inputs
  beyond ±2^30 are clamped before the lattice lookup, so huge or infinite
  coordinates still produce finite in-range values. NaN input yields an
  unspecified finite value.

The hot path contains no `floorf`/`fmodf` calls or vector ops; it is scalar
integer hashing plus a handful of multiplies, roughly 7x faster than the
previous GLSL-derived implementation on SSE2 builds and 4x on AVX2 builds.

**Versioned output change**: earlier releases ported the GLSL `mod289`
float-hash Perlin noise, whose values differed and could exceed ±1 (up to
about ±1.2 in 3D/4D). Anything that baked noise from the old implementation
(terrain, textures, seeds) will see different values after upgrading.

## Performance

The runnable [math benchmark](../benchmarks/bench_math.cpp) covers all 15
operations in the weighted profile below, plus camera construction, quaternion
extraction, decomposition, frustum extraction, inverse transforms, and aligned
four-matrix multiplication:

```bat
build.bat bench_math
.bench-build\current\bench_math.exe quat_from_mat
```

The optional argument filters case names by substring. Each case runs one
warmup and seven samples, reporting `BENCH name sample_index ns_per_op checksum`.
Samples process batches of 8,192 varying inputs until at least 20 milliseconds
of computation have been measured. Preparation and full-output consumption
are outside each timed batch. The consumer is compiled separately without
whole-program optimization so every result remains observable. Reported times
include array loads/stores and loop overhead; the libraries use their native
vector and matrix layouts.

The harness uses C++17 for comparison with C++ math libraries; the supported
library interface is C. The standalone harness needs no comparison dependencies.
With the supplied build command, enable optional comparisons through environment
variables:

```bat
set RG_BENCH_DEPS=C:\path\to\benchmark-dependencies
set RG_BENCH_DIRECTXMATH=1
build.bat bench_math
```

The builder enables cglm when `RG_BENCH_DEPS` contains
`cglm/include/cglm/cglm.h`, and GLM when it contains `glm/glm.hpp`.
`RG_BENCH_DIRECTXMATH=1` enables DirectXMath from the installed Windows SDK.
Each enabled library adds the same 15 profile operations.

For custom builds, the corresponding definitions are `RG_BENCH_CGLM`,
`RG_BENCH_DIRECTXMATH`, and `RG_BENCH_GLM`; provide the cglm `include` directory,
the Windows SDK, or the directory containing `glm/` on the include path.
All use the same inputs and timing protocol; the default library normalization
precision is retained. These comparisons therefore include each library's
precision/safety tradeoffs.

### Linux x86-64 results

Measured with the checked-in harness on a low-end laptop: Intel Core i3-1005G1
(Ice Lake, 2 cores), Ubuntu 24.04, GCC 13.3, `./build.sh bench_math` (`-O2
-DNDEBUG`; the AVX2 column is the `bench_math_avx2` binary built with `-mavx2
-mfma`). Default configuration (`RG_MATH_MAX_PERF=1`, refined `rsqrt`).
Nanoseconds per operation, median of seven samples and three process runs on
one pinned core; includes the harness's array loads and stores.

| Operation | SSE2 | AVX2+FMA | | Operation | SSE2 | AVX2+FMA |
| --- | ---: | ---: | --- | --- | ---: | ---: |
| `vec3_add` | 0.64 | 0.66 | | `mat4_mul` | 4.75 | 3.23 |
| `vec3_scale` | 0.59 | 0.59 | | `mat4_mulv4` | 1.60 | 2.37 |
| `vec3_dot` | 0.85 | 1.04 | | `mat4_mul4` (4 products) | 16.0 | 9.1 |
| `vec3_length` | 0.90 | 0.91 | | `mat4_inv_tr` | 2.43 | 2.39 |
| `vec3_normalize` | 1.84 | 1.76 | | `mat4_inv_affine` | 7.00 | 8.37 |
| `vec3_cross` | 0.91 | 0.96 | | `mat4_inv` | 9.19 | 7.79 |
| `vec4_dot` | 0.85 | 0.98 | | `mat4_trs_make` | 3.48 | 3.01 |
| `vec4_length` | 0.90 | 0.95 | | `mat4_from_quat` | 3.58 | 2.90 |
| `vec4_normalize` | 1.73 | 1.47 | | `mat4_decompose` | 11.7 | 10.0 |
| `vec4_floor` | 1.41 | 0.39 | | `look_at_rh` | 16.4 | 10.4 |
| `vec4_sqrt` | 0.89 | 0.89 | | `frustum_extract` | 19.9 | 16.6 |
| `quat_mul` | 1.81 | 1.24 | | `quat_from_mat4` | 3.01 | 2.92 |
| `quat_normalize` | 1.82 | 1.54 | | `aabb_transform` | 3.09 | 2.64 |
| `quat_slerp` | 15.9 | 9.3 | | `aabb_in_frustum` | 4.88 | 2.46 |
| `quat_nlerp` | 4.98 | 4.43 | | `aabb_in_frustum_soa` | 4.28 | 2.16 |
| `ray_sphere` hit / miss | 6.8 / 4.4 | 5.6 / 2.9 | | `frustum_cull_aabbs`, per box | 4.4 | 2.2 |
| `ray_aabb` hit / miss | 6.0 / 5.5 | 5.8 / 5.1 | | `sphere_in_frustum` / `_soa` | 3.0 / 2.7 | 1.6 / 1.5 |
| `ray_inv_aabb` | 4.61 | 4.37 | | `frustum_cull_spheres`, per sphere | 2.7 | 1.6 |
| `ray_triangle` hit / miss | 9.7 / 11.8 | 7.8 / 6.1 | | `perlin_vec2` / `vec3` / `vec4` | 11.4 / 22.4 / 41.3 | 8.7 / 19.7 / 34.1 |

Applying the weights of the 90-point profile below to these medians gives
1.53 ns per weighted operation on the SSE2 build and 1.43 ns on the AVX2 build.
The normalization rows include the Newton-Raphson refinement;
`RG_MATH_RSQRT_MODE_RSQRT_RAW` returns roughly to the pre-refinement figures
(`vec3_normalize` about 1.4 ns, `look_at_rh` about 13.6 ns) at 12-bit
precision. No comparison libraries were present on this machine; the MSVC
comparison tables follow.

The following charts and tables are **historical results**.
Use the runnable harness for comparisons of current builds; its batch methodology
differs from the historical measurements, which do not establish current performance.

![rg_math hot-path benchmark results](benchmarks/rg_math-hot-path.svg)

The upper panel compares `rg_math` with [cglm 0.9.6](https://github.com/recp/cglm)
across representative vector, matrix, and quaternion operations. Each value is
the median elapsed time in milliseconds for 1,000,000 calls, so lower is
better.

| Operation | rg_math | cglm |
| --- | ---: | ---: |
| `vec3_add` | 0.39 ms | 0.39 ms |
| `vec3_dot` | 0.92 ms | 1.24 ms |
| `vec3_length` | 1.27 ms | 1.30 ms |
| `vec3_normalize` | 2.05 ms | 2.53 ms |
| `vec4_add` | 1.01 ms | 1.00 ms |
| `mat4_mul` | 0.38 ms | 2.56 ms |
| `mat4_mulv4` | 2.20 ms | 2.21 ms |
| `quat_mul` | 0.40 ms | 0.40 ms |
| `quat_normalize` | 2.57 ms | 3.51 ms |

The lower panel summarizes a CPU-side 3D-engine profile covering transform
updates, animation, physics, and culling. It is a weighted model rather than a
trace from one application:

| Area | Operations and weights |
| --- | --- |
| Vec3 (45) | add 8, scale 8, dot 10, length 7, normalize 10, cross 2 |
| Mat4 (20) | multiply 8, matrix-vector multiply 12 |
| Quaternion (15) | multiply 9, normalize 6 |
| Vec4 (10) | add 3, scale 3, dot 2, length 1, normalize 1 |

The weighted score is the sum of each median multiplied by its weight, divided
by the 90-point shared-profile total. Only libraries with a measurement for
every operation are shown:

| Library | Weighted median | Coverage |
| --- | ---: | ---: |
| `rg_math` | 1.14 ms | 100% |
| [DirectXMath 3.19](https://github.com/microsoft/DirectXMath) | 1.48 ms | 100% |
| [cglm 0.9.6](https://github.com/recp/cglm) | 1.50 ms | 100% |
| [GLM 1.1.0](https://github.com/g-truc/glm) | 3.65 ms | 100% |

The then-current public headers and all comparison libraries were built into the
same C++17 benchmark executable with MSVC 19.44.35217 for x64 using `/O2 /Ob3
/Oi /Ot /Oy /GL /LTCG /arch:AVX2 /fp:fast /GS- /DNDEBUG`. The default
`RG_MATH_MAX_PERF=1`, `RG_MATH_USE_LIBC=1`, and `RG_MATH_LIBC_ALIASES=1`
configuration was used. Each benchmark group ran in a fresh process; the
figure reports the median of seven runs after a 10,000-call warmup per
operation.

Measurements were taken on an AMD Ryzen 9 4900HS on Windows build 26200.9168.
Results are machine-specific and should not be treated as a performance guarantee.

## Build verification

From a Visual Studio Developer Command Prompt:

```bat
build.bat test_math
```

The target checks C builds with baseline x64 SIMD, AVX2, checked SIMD and scalar,
plain-layout, reduced-umbrella, and all four clip-space configurations.
Tests cover camera-space invariants, quaternion/matrix and signed-TRS round
trips, asymmetric projection corners, frustum culling, and inverse/multiply
aliasing and alignment contracts, along with representative functionality from
every module. Benchmarks run separately from correctness tests.
