// rg_math_noise - perlin noise helpers
//
// Part of the Reverse Gravity (rg_) core libraries.
// Provides classic (improved, 2002) Perlin gradient noise in two through four
// dimensions. The lattice repeats every 256 units on each axis.
//
// Author: Steven Wendel (superwendel)

#ifndef RG_MATH_NOISE_H
#define RG_MATH_NOISE_H

#include "rg_math_vec.h"

RG_MATH_EXTERN_C_BEGIN

// =============================================================================
// NOISE HELPERS
// =============================================================================
//
// All three functions return deterministic gradient noise in [-1, 1] that is
// exactly 0 at integer lattice points and C2-continuous across cell
// boundaries (quintic fade). Coordinates are evaluated in f32: a coordinate
// above 2^24 in magnitude is always integral, so the noise degenerates along
// that axis (and is 0 once every coordinate is integral); coordinates beyond
// +-2^30 are clamped before the lattice lookup so no finite input can produce
// an out-of-range value. NaN input yields an unspecified finite value.

RGINLINE f32 rg_perlin_vec4(const rg_vec4* point);
RGINLINE f32 rg_perlin_vec3(const rg_vec3* point);
RGINLINE f32 rg_perlin_vec2(const rg_vec2* point);

RG_MATH_EXTERN_C_END

// =============================================================================
// IMPLEMENTATION
// =============================================================================

// Ken Perlin's reference permutation, doubled so that p[p[X] + Y + 1] and
// friends never need masking (max index 255 + 255 + 1 = 511).
static const u8 rg_noise_perm[512] = {
	151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148,
	247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175,
	74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54,
	65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64,
	52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213,
	119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104,
	218, 246, 97, 228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157,
	184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180,
	151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148,
	247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175,
	74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244, 102, 143, 54,
	65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196, 135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64,
	52, 217, 226, 250, 124, 123, 5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42, 223, 183, 170, 213,
	119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9, 129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104,
	218, 246, 97, 228, 251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107, 49, 192, 214, 31, 181, 199, 106, 157,
	184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254, 138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
};

// 2D: the four diagonals and four axis directions (hash & 7).
static const f32 rg_noise_grad2[8][2] = {
	{ 1.0f, 1.0f }, { -1.0f, 1.0f }, { 1.0f, -1.0f }, { -1.0f, -1.0f },
	{ 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f }
};

// 3D: the twelve cube-edge midpoints, padded to 16 entries with four repeats
// exactly as in the reference implementation (hash & 15).
static const f32 rg_noise_grad3[16][4] = {
	{ 1.0f, 1.0f, 0.0f, 0.0f }, { -1.0f, 1.0f, 0.0f, 0.0f }, { 1.0f, -1.0f, 0.0f, 0.0f }, { -1.0f, -1.0f, 0.0f, 0.0f },
	{ 1.0f, 0.0f, 1.0f, 0.0f }, { -1.0f, 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, -1.0f, 0.0f },
	{ 0.0f, 1.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, -1.0f, 0.0f }, { 0.0f, -1.0f, -1.0f, 0.0f },
	{ 1.0f, 1.0f, 0.0f, 0.0f }, { -1.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, -1.0f, 0.0f }
};

// 4D: the 32 tesseract-edge midpoints (one zero coordinate, the rest +-1),
// hash & 31.
static const f32 rg_noise_grad4[32][4] = {
	{ 0.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, -1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, -1.0f, 1.0f }, { 0.0f, -1.0f, -1.0f, 1.0f },
	{ 0.0f, 1.0f, 1.0f, -1.0f }, { 0.0f, -1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, -1.0f, -1.0f },
	{ 1.0f, 0.0f, 1.0f, 1.0f }, { -1.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, -1.0f, 1.0f }, { -1.0f, 0.0f, -1.0f, 1.0f },
	{ 1.0f, 0.0f, 1.0f, -1.0f }, { -1.0f, 0.0f, 1.0f, -1.0f }, { 1.0f, 0.0f, -1.0f, -1.0f }, { -1.0f, 0.0f, -1.0f, -1.0f },
	{ 1.0f, 1.0f, 0.0f, 1.0f }, { -1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, -1.0f, 0.0f, 1.0f }, { -1.0f, -1.0f, 0.0f, 1.0f },
	{ 1.0f, 1.0f, 0.0f, -1.0f }, { -1.0f, 1.0f, 0.0f, -1.0f }, { 1.0f, -1.0f, 0.0f, -1.0f }, { -1.0f, -1.0f, 0.0f, -1.0f },
	{ 1.0f, 1.0f, 1.0f, 0.0f }, { -1.0f, 1.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, 1.0f, 0.0f }, { -1.0f, -1.0f, 1.0f, 0.0f },
	{ 1.0f, 1.0f, -1.0f, 0.0f }, { -1.0f, 1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, -1.0f, 0.0f }, { -1.0f, -1.0f, -1.0f, 0.0f }
};

// Output scales. The raw noise is bounded by sum_c w_c * max_g (g . d_c),
// which peaks at 1.0 (2D), 1.036354 (3D) and 1.536582 (4D) for the gradient
// sets above. Each scale is 1/peak rounded down by at least 1e-4 relative so
// f32 rounding can never push a result past +-1.
#define RG_NOISE_SCALE2 0.9999f
#define RG_NOISE_SCALE3 0.9649f
#define RG_NOISE_SCALE4 0.6507f

// Coordinates are clamped to +-2^30 so the f32 -> i32 conversion below is
// always defined. Inputs beyond 2^24 are integral anyway (noise is 0 there).
#define RG_NOISE_COORD_LIMIT 1073741824.0f

// Branch-free floor: truncate, then step down when the input was negative and
// non-integral. Writes the fractional part and returns the lattice cell.
RGINLINE i32 rg_noise_floor(f32 x, f32* fract)
{
	x = x > RG_NOISE_COORD_LIMIT ? RG_NOISE_COORD_LIMIT : x;
	x = x < -RG_NOISE_COORD_LIMIT ? -RG_NOISE_COORD_LIMIT : x;
	i32 i = (i32)x;
	i -= (i32)(x < (f32)i);
	*fract = x - (f32)i;
	return i;
}

RGINLINE f32 rg_noise_fade(f32 t)
{
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

RGINLINE f32 rg_noise_lerp(f32 a, f32 b, f32 t)
{
	return a + t * (b - a);
}

RGINLINE f32 rg_noise_grad_dot2(u32 hash, f32 x, f32 y)
{
	const f32* g = rg_noise_grad2[hash & 7u];
	return g[0] * x + g[1] * y;
}

RGINLINE f32 rg_noise_grad_dot3(u32 hash, f32 x, f32 y, f32 z)
{
	const f32* g = rg_noise_grad3[hash & 15u];
	return g[0] * x + g[1] * y + g[2] * z;
}

RGINLINE f32 rg_noise_grad_dot4(u32 hash, f32 x, f32 y, f32 z, f32 w)
{
	const f32* g = rg_noise_grad4[hash & 31u];
	return g[0] * x + g[1] * y + g[2] * z + g[3] * w;
}

RGINLINE f32 rg_perlin_vec2(const rg_vec2* point)
{
	const u8* p = rg_noise_perm;
	f32 x, y;
	u32 xi = (u32)rg_noise_floor(point->x, &x) & 255u;
	u32 yi = (u32)rg_noise_floor(point->y, &y) & 255u;

	f32 u = rg_noise_fade(x);
	f32 v = rg_noise_fade(y);

	u32 a = (u32)p[xi] + yi;
	u32 b = (u32)p[xi + 1u] + yi;

	f32 n00 = rg_noise_grad_dot2(p[a], x, y);
	f32 n10 = rg_noise_grad_dot2(p[b], x - 1.0f, y);
	f32 n01 = rg_noise_grad_dot2(p[a + 1u], x, y - 1.0f);
	f32 n11 = rg_noise_grad_dot2(p[b + 1u], x - 1.0f, y - 1.0f);

	f32 nx0 = rg_noise_lerp(n00, n10, u);
	f32 nx1 = rg_noise_lerp(n01, n11, u);
	return rg_noise_lerp(nx0, nx1, v) * RG_NOISE_SCALE2;
}

RGINLINE f32 rg_perlin_vec3(const rg_vec3* point)
{
	const u8* p = rg_noise_perm;
	f32 x, y, z;
	u32 xi = (u32)rg_noise_floor(point->x, &x) & 255u;
	u32 yi = (u32)rg_noise_floor(point->y, &y) & 255u;
	u32 zi = (u32)rg_noise_floor(point->z, &z) & 255u;

	f32 u = rg_noise_fade(x);
	f32 v = rg_noise_fade(y);
	f32 w = rg_noise_fade(z);

	u32 a = (u32)p[xi] + yi;
	u32 b = (u32)p[xi + 1u] + yi;
	u32 aa = (u32)p[a] + zi;
	u32 ab = (u32)p[a + 1u] + zi;
	u32 ba = (u32)p[b] + zi;
	u32 bb = (u32)p[b + 1u] + zi;

	f32 x1 = x - 1.0f;
	f32 y1 = y - 1.0f;
	f32 z1 = z - 1.0f;

	f32 n000 = rg_noise_grad_dot3(p[aa], x, y, z);
	f32 n100 = rg_noise_grad_dot3(p[ba], x1, y, z);
	f32 n010 = rg_noise_grad_dot3(p[ab], x, y1, z);
	f32 n110 = rg_noise_grad_dot3(p[bb], x1, y1, z);
	f32 n001 = rg_noise_grad_dot3(p[aa + 1u], x, y, z1);
	f32 n101 = rg_noise_grad_dot3(p[ba + 1u], x1, y, z1);
	f32 n011 = rg_noise_grad_dot3(p[ab + 1u], x, y1, z1);
	f32 n111 = rg_noise_grad_dot3(p[bb + 1u], x1, y1, z1);

	f32 nx00 = rg_noise_lerp(n000, n100, u);
	f32 nx10 = rg_noise_lerp(n010, n110, u);
	f32 nx01 = rg_noise_lerp(n001, n101, u);
	f32 nx11 = rg_noise_lerp(n011, n111, u);
	f32 nxy0 = rg_noise_lerp(nx00, nx10, v);
	f32 nxy1 = rg_noise_lerp(nx01, nx11, v);
	return rg_noise_lerp(nxy0, nxy1, w) * RG_NOISE_SCALE3;
}

RGINLINE f32 rg_perlin_vec4(const rg_vec4* point)
{
	const u8* p = rg_noise_perm;
	f32 x, y, z, w;
	u32 xi = (u32)rg_noise_floor(point->x, &x) & 255u;
	u32 yi = (u32)rg_noise_floor(point->y, &y) & 255u;
	u32 zi = (u32)rg_noise_floor(point->z, &z) & 255u;
	u32 wi = (u32)rg_noise_floor(point->w, &w) & 255u;

	f32 fu = rg_noise_fade(x);
	f32 fv = rg_noise_fade(y);
	f32 fw = rg_noise_fade(z);
	f32 ft = rg_noise_fade(w);

	u32 a = (u32)p[xi] + yi;
	u32 b = (u32)p[xi + 1u] + yi;
	u32 aa = (u32)p[a] + zi;
	u32 ab = (u32)p[a + 1u] + zi;
	u32 ba = (u32)p[b] + zi;
	u32 bb = (u32)p[b + 1u] + zi;
	u32 aaa = (u32)p[aa] + wi;
	u32 aab = (u32)p[aa + 1u] + wi;
	u32 aba = (u32)p[ab] + wi;
	u32 abb = (u32)p[ab + 1u] + wi;
	u32 baa = (u32)p[ba] + wi;
	u32 bab = (u32)p[ba + 1u] + wi;
	u32 bba = (u32)p[bb] + wi;
	u32 bbb = (u32)p[bb + 1u] + wi;

	f32 x1 = x - 1.0f;
	f32 y1 = y - 1.0f;
	f32 z1 = z - 1.0f;
	f32 w1 = w - 1.0f;

	f32 n0000 = rg_noise_grad_dot4(p[aaa], x, y, z, w);
	f32 n1000 = rg_noise_grad_dot4(p[baa], x1, y, z, w);
	f32 n0100 = rg_noise_grad_dot4(p[aba], x, y1, z, w);
	f32 n1100 = rg_noise_grad_dot4(p[bba], x1, y1, z, w);
	f32 n0010 = rg_noise_grad_dot4(p[aab], x, y, z1, w);
	f32 n1010 = rg_noise_grad_dot4(p[bab], x1, y, z1, w);
	f32 n0110 = rg_noise_grad_dot4(p[abb], x, y1, z1, w);
	f32 n1110 = rg_noise_grad_dot4(p[bbb], x1, y1, z1, w);
	f32 n0001 = rg_noise_grad_dot4(p[aaa + 1u], x, y, z, w1);
	f32 n1001 = rg_noise_grad_dot4(p[baa + 1u], x1, y, z, w1);
	f32 n0101 = rg_noise_grad_dot4(p[aba + 1u], x, y1, z, w1);
	f32 n1101 = rg_noise_grad_dot4(p[bba + 1u], x1, y1, z, w1);
	f32 n0011 = rg_noise_grad_dot4(p[aab + 1u], x, y, z1, w1);
	f32 n1011 = rg_noise_grad_dot4(p[bab + 1u], x1, y, z1, w1);
	f32 n0111 = rg_noise_grad_dot4(p[abb + 1u], x, y1, z1, w1);
	f32 n1111 = rg_noise_grad_dot4(p[bbb + 1u], x1, y1, z1, w1);

	f32 nx000 = rg_noise_lerp(n0000, n1000, fu);
	f32 nx100 = rg_noise_lerp(n0100, n1100, fu);
	f32 nx010 = rg_noise_lerp(n0010, n1010, fu);
	f32 nx110 = rg_noise_lerp(n0110, n1110, fu);
	f32 nx001 = rg_noise_lerp(n0001, n1001, fu);
	f32 nx101 = rg_noise_lerp(n0101, n1101, fu);
	f32 nx011 = rg_noise_lerp(n0011, n1011, fu);
	f32 nx111 = rg_noise_lerp(n0111, n1111, fu);

	f32 nxy00 = rg_noise_lerp(nx000, nx100, fv);
	f32 nxy10 = rg_noise_lerp(nx010, nx110, fv);
	f32 nxy01 = rg_noise_lerp(nx001, nx101, fv);
	f32 nxy11 = rg_noise_lerp(nx011, nx111, fv);

	f32 nxyz0 = rg_noise_lerp(nxy00, nxy10, fw);
	f32 nxyz1 = rg_noise_lerp(nxy01, nxy11, fw);
	return rg_noise_lerp(nxyz0, nxyz1, ft) * RG_NOISE_SCALE4;
}

#undef RG_NOISE_SCALE2
#undef RG_NOISE_SCALE3
#undef RG_NOISE_SCALE4
#undef RG_NOISE_COORD_LIMIT

#endif // RG_MATH_NOISE_H
