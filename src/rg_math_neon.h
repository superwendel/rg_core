// rg_math_neon - NEON implementation of the SSE intrinsic subset used by rg_math
//
// Part of the Reverse Gravity (rg_) core libraries.
// rg_math's SIMD paths are written against the SSE vocabulary (__m128,
// _mm_add_ps, _mm_shuffle_ps, ...). On AArch64 this header provides that
// vocabulary on top of NEON so the same code compiles to NEON instructions.
// It is included by rg_math_core.h when RG_MATH_NEON is selected and is not
// meant to be included directly.
//
// Only the intrinsics rg_math uses are provided. Semantics follow SSE with
// these documented differences:
//   - _mm_min_ps/_mm_max_ps return NaN when either input is NaN (SSE returns
//     the second operand). rg_math does not rely on the SSE behaviour.
//   - _mm_cvttps_epi32 saturates out-of-range values instead of producing
//     0x80000000.
//   - The reciprocal and reciprocal square root estimates are refined with
//     one Newton-Raphson step so they are at least as accurate as the SSE
//     estimates they replace (about 16 bits instead of 12).
//   - The *_ss operations compute lane 0 and copy lanes 1-3 from the first
//     operand, exactly as SSE does. They evaluate only lane 0, so unrelated
//     lanes (such as a negative value next to a square root) raise no
//     floating-point exceptions.
// Requires GCC 12+ or Clang (__builtin_shufflevector); MSVC ARM64 builds use
// the scalar implementation.
//
// Author: Steven Wendel (superwendel)

#ifndef RG_MATH_NEON_H
#define RG_MATH_NEON_H

#include <arm_neon.h>
#include <stdint.h>
#include <string.h>

#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 12
#error rg_math_neon.h requires GCC 12 or later (or Clang); define RG_MATH_NO_SIMD for older compilers
#endif

#ifndef RGINLINE
#define RGINLINE static inline __attribute__((always_inline))
#endif

typedef float32x4_t __m128;
typedef int32x4_t __m128i;

#define _MM_SHUFFLE(z, y, x, w) (((z) << 6) | ((y) << 4) | ((x) << 2) | (w))
#define _MM_FROUND_TO_NEAREST_INT 0x00
#define _MM_FROUND_TO_NEG_INF 0x01
#define _MM_FROUND_TO_POS_INF 0x02
#define _MM_FROUND_TO_ZERO 0x03
#define _MM_FROUND_NO_EXC 0x08

// -----------------------------------------------------------------------------
// Loads, stores, broadcasts
// -----------------------------------------------------------------------------

RGINLINE __m128 _mm_load_ps(const float* p) { return vld1q_f32(p); }
RGINLINE __m128 _mm_loadu_ps(const float* p) { return vld1q_f32(p); }
RGINLINE void _mm_store_ps(float* p, __m128 a) { vst1q_f32(p, a); }
RGINLINE void _mm_storeu_ps(float* p, __m128 a) { vst1q_f32(p, a); }

// 64-bit loads and stores go through memcpy so they may alias any lane type.
RGINLINE __m128i _mm_loadl_epi64(const __m128i* p)
{
	uint64_t bits;
	memcpy(&bits, p, sizeof(bits));
	return vreinterpretq_s32_u64(vcombine_u64(vcreate_u64(bits), vdup_n_u64(0)));
}

RGINLINE void _mm_storel_epi64(__m128i* p, __m128i a)
{
	uint64_t bits = vgetq_lane_u64(vreinterpretq_u64_s32(a), 0);
	memcpy(p, &bits, sizeof(bits));
}

RGINLINE __m128 _mm_setzero_ps(void) { return vdupq_n_f32(0.0f); }
RGINLINE __m128 _mm_set1_ps(float a) { return vdupq_n_f32(a); }
RGINLINE __m128 _mm_set_ss(float a) { return vsetq_lane_f32(a, vdupq_n_f32(0.0f), 0); }

RGINLINE __m128 _mm_setr_ps(float e0, float e1, float e2, float e3)
{
	float32x4_t r = {e0, e1, e2, e3};
	return r;
}

RGINLINE __m128 _mm_set_ps(float e3, float e2, float e1, float e0)
{
	float32x4_t r = {e0, e1, e2, e3};
	return r;
}

RGINLINE __m128i _mm_set1_epi32(int a) { return vdupq_n_s32(a); }

RGINLINE __m128i _mm_setr_epi32(int e0, int e1, int e2, int e3)
{
	int32x4_t r = {e0, e1, e2, e3};
	return r;
}

RGINLINE __m128i _mm_set_epi32(int e3, int e2, int e1, int e0)
{
	int32x4_t r = {e0, e1, e2, e3};
	return r;
}

// -----------------------------------------------------------------------------
// Casts and conversions
// -----------------------------------------------------------------------------

RGINLINE __m128 _mm_castsi128_ps(__m128i a) { return vreinterpretq_f32_s32(a); }
RGINLINE __m128i _mm_castps_si128(__m128 a) { return vreinterpretq_s32_f32(a); }
RGINLINE __m128i _mm_cvttps_epi32(__m128 a) { return vcvtq_s32_f32(a); }
RGINLINE __m128 _mm_cvtepi32_ps(__m128i a) { return vcvtq_f32_s32(a); }
RGINLINE float _mm_cvtss_f32(__m128 a) { return vgetq_lane_f32(a, 0); }

// -----------------------------------------------------------------------------
// Packed arithmetic
// -----------------------------------------------------------------------------

RGINLINE __m128 _mm_add_ps(__m128 a, __m128 b) { return vaddq_f32(a, b); }
RGINLINE __m128 _mm_sub_ps(__m128 a, __m128 b) { return vsubq_f32(a, b); }
RGINLINE __m128 _mm_mul_ps(__m128 a, __m128 b) { return vmulq_f32(a, b); }
RGINLINE __m128 _mm_div_ps(__m128 a, __m128 b) { return vdivq_f32(a, b); }
RGINLINE __m128 _mm_min_ps(__m128 a, __m128 b) { return vminq_f32(a, b); }
RGINLINE __m128 _mm_max_ps(__m128 a, __m128 b) { return vmaxq_f32(a, b); }
RGINLINE __m128 _mm_sqrt_ps(__m128 a) { return vsqrtq_f32(a); }
RGINLINE __m128i _mm_add_epi32(__m128i a, __m128i b) { return vaddq_s32(a, b); }
RGINLINE __m128i _mm_sub_epi32(__m128i a, __m128i b) { return vsubq_s32(a, b); }

// a * b + c and c - a * b, fused.
RGINLINE __m128 _mm_fmadd_ps(__m128 a, __m128 b, __m128 c) { return vfmaq_f32(c, a, b); }
RGINLINE __m128 _mm_fnmadd_ps(__m128 a, __m128 b, __m128 c) { return vfmsq_f32(c, a, b); }

// Hardware estimate refined by one Newton-Raphson step (about 2^-16 relative
// error, versus 1.5 * 2^-12 for the SSE instructions).
RGINLINE __m128 _mm_rcp_ps(__m128 a)
{
	float32x4_t e = vrecpeq_f32(a);
	return vmulq_f32(e, vrecpsq_f32(a, e));
}

RGINLINE __m128 _mm_rsqrt_ps(__m128 a)
{
	float32x4_t e = vrsqrteq_f32(a);
	return vmulq_f32(e, vrsqrtsq_f32(vmulq_f32(a, e), e));
}

// -----------------------------------------------------------------------------
// Scalar (lane 0) arithmetic
// -----------------------------------------------------------------------------

RGINLINE __m128 rg__neon_insert0(__m128 a, float32x2_t lo)
{
	return vsetq_lane_f32(vget_lane_f32(lo, 0), a, 0);
}

// Lane 0 of a broadcast to both halves of a d register, so two-lane
// instructions never operate on an unrelated value.
RGINLINE float32x2_t rg__neon_lane0(__m128 a)
{
	return vdup_n_f32(vgetq_lane_f32(a, 0));
}

RGINLINE __m128 _mm_add_ss(__m128 a, __m128 b) { return vsetq_lane_f32(vgetq_lane_f32(a, 0) + vgetq_lane_f32(b, 0), a, 0); }
RGINLINE __m128 _mm_sub_ss(__m128 a, __m128 b) { return vsetq_lane_f32(vgetq_lane_f32(a, 0) - vgetq_lane_f32(b, 0), a, 0); }
RGINLINE __m128 _mm_mul_ss(__m128 a, __m128 b) { return vsetq_lane_f32(vgetq_lane_f32(a, 0) * vgetq_lane_f32(b, 0), a, 0); }
RGINLINE __m128 _mm_div_ss(__m128 a, __m128 b) { return vsetq_lane_f32(vgetq_lane_f32(a, 0) / vgetq_lane_f32(b, 0), a, 0); }
RGINLINE __m128 _mm_min_ss(__m128 a, __m128 b) { return rg__neon_insert0(a, vmin_f32(rg__neon_lane0(a), rg__neon_lane0(b))); }
RGINLINE __m128 _mm_max_ss(__m128 a, __m128 b) { return rg__neon_insert0(a, vmax_f32(rg__neon_lane0(a), rg__neon_lane0(b))); }
RGINLINE __m128 _mm_sqrt_ss(__m128 a) { return rg__neon_insert0(a, vsqrt_f32(rg__neon_lane0(a))); }

// The scalar estimate forms keep lane 0 in a scalar register so a following
// lane broadcast needs no insert.
RGINLINE __m128 _mm_rcp_ss(__m128 a)
{
	float x = vgetq_lane_f32(a, 0);
	float e = vrecpes_f32(x);
	return vsetq_lane_f32(e * vrecpss_f32(x, e), a, 0);
}

RGINLINE __m128 _mm_rsqrt_ss(__m128 a)
{
	float x = vgetq_lane_f32(a, 0);
	float e = vrsqrtes_f32(x);
	return vsetq_lane_f32(e * vrsqrtss_f32(x * e, e), a, 0);
}

// -----------------------------------------------------------------------------
// Rounding (SSE4.1)
// -----------------------------------------------------------------------------

RGINLINE __m128 _mm_floor_ps(__m128 a) { return vrndmq_f32(a); }
RGINLINE __m128 _mm_floor_ss(__m128 a, __m128 b) { return rg__neon_insert0(a, vrndm_f32(rg__neon_lane0(b))); }
RGINLINE __m128 _mm_ceil_ss(__m128 a, __m128 b) { return rg__neon_insert0(a, vrndp_f32(rg__neon_lane0(b))); }

RGINLINE __m128 _mm_round_ss(__m128 a, __m128 b, int rounding)
{
	float32x2_t x = rg__neon_lane0(b);
	switch (rounding & 0x3)
	{
	case _MM_FROUND_TO_NEG_INF: return rg__neon_insert0(a, vrndm_f32(x));
	case _MM_FROUND_TO_POS_INF: return rg__neon_insert0(a, vrndp_f32(x));
	case _MM_FROUND_TO_ZERO: return rg__neon_insert0(a, vrnd_f32(x));
	default: return rg__neon_insert0(a, vrndn_f32(x));
	}
}

// -----------------------------------------------------------------------------
// Comparisons and bitwise logic
// -----------------------------------------------------------------------------

RGINLINE __m128 _mm_cmpeq_ps(__m128 a, __m128 b) { return vreinterpretq_f32_u32(vceqq_f32(a, b)); }
RGINLINE __m128 _mm_cmplt_ps(__m128 a, __m128 b) { return vreinterpretq_f32_u32(vcltq_f32(a, b)); }
RGINLINE __m128 _mm_cmple_ps(__m128 a, __m128 b) { return vreinterpretq_f32_u32(vcleq_f32(a, b)); }
RGINLINE __m128 _mm_cmpgt_ps(__m128 a, __m128 b) { return vreinterpretq_f32_u32(vcgtq_f32(a, b)); }
RGINLINE __m128 _mm_cmpge_ps(__m128 a, __m128 b) { return vreinterpretq_f32_u32(vcgeq_f32(a, b)); }
// Not-less-than is true for unordered operands, unlike greater-or-equal.
RGINLINE __m128 _mm_cmpnlt_ps(__m128 a, __m128 b) { return vreinterpretq_f32_u32(vmvnq_u32(vcltq_f32(a, b))); }

RGINLINE __m128 _mm_and_ps(__m128 a, __m128 b)
{
	return vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
}

// (~a) & b
RGINLINE __m128 _mm_andnot_ps(__m128 a, __m128 b)
{
	return vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(b), vreinterpretq_u32_f32(a)));
}

RGINLINE __m128 _mm_or_ps(__m128 a, __m128 b)
{
	return vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
}

RGINLINE __m128 _mm_xor_ps(__m128 a, __m128 b)
{
	return vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(a), vreinterpretq_u32_f32(b)));
}

RGINLINE __m128i _mm_and_si128(__m128i a, __m128i b) { return vandq_s32(a, b); }

RGINLINE int _mm_movemask_ps(__m128 a)
{
	const int32x4_t shifts = {0, 1, 2, 3};
	uint32x4_t bits = vshrq_n_u32(vreinterpretq_u32_f32(a), 31);
	return (int)vaddvq_u32(vshlq_u32(bits, shifts));
}

// -----------------------------------------------------------------------------
// Permutes
// -----------------------------------------------------------------------------

// Lanes 0-1 from a, lanes 2-3 from b, each pair selected by a 2-bit field.
#define _mm_shuffle_ps(a, b, imm) \
	__builtin_shufflevector((a), (b), ((imm) & 3), (((imm) >> 2) & 3), ((((imm) >> 4) & 3) + 4), ((((imm) >> 6) & 3) + 4))

#define _mm_shuffle_epi32(a, imm) \
	__builtin_shufflevector((a), (a), ((imm) & 3), (((imm) >> 2) & 3), (((imm) >> 4) & 3), (((imm) >> 6) & 3))

// Lane i comes from b when bit i of imm is set, otherwise from a.
#define _mm_blend_ps(a, b, imm) \
	__builtin_shufflevector((a), (b), (((imm) & 1) ? 4 : 0), (((imm) & 2) ? 5 : 1), (((imm) & 4) ? 6 : 2), (((imm) & 8) ? 7 : 3))

// {b2, b3, a2, a3}
RGINLINE __m128 _mm_movehl_ps(__m128 a, __m128 b) { return vcombine_f32(vget_high_f32(b), vget_high_f32(a)); }
// {a0, a1, b0, b1}
RGINLINE __m128 _mm_movelh_ps(__m128 a, __m128 b) { return vcombine_f32(vget_low_f32(a), vget_low_f32(b)); }

#define _MM_TRANSPOSE4_PS(row0, row1, row2, row3) \
	do \
	{ \
		float32x4x2_t rg__t01 = vtrnq_f32((row0), (row1)); \
		float32x4x2_t rg__t23 = vtrnq_f32((row2), (row3)); \
		(row0) = vcombine_f32(vget_low_f32(rg__t01.val[0]), vget_low_f32(rg__t23.val[0])); \
		(row1) = vcombine_f32(vget_low_f32(rg__t01.val[1]), vget_low_f32(rg__t23.val[1])); \
		(row2) = vcombine_f32(vget_high_f32(rg__t01.val[0]), vget_high_f32(rg__t23.val[0])); \
		(row3) = vcombine_f32(vget_high_f32(rg__t01.val[1]), vget_high_f32(rg__t23.val[1])); \
	} while (0)

#endif // RG_MATH_NEON_H
