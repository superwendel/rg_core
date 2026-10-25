// rg_math public API correctness tests

// Route library assertions to a counter so contract violations are testable.
static void math_assert_failed(const char* expression);
#define RG_MATH_ASSERT(x) ((x) ? (void)0 : math_assert_failed(#x))

#include "../src/rg_math.h"
#include "../src/rg_math.h"

#include <fenv.h>
#include <math.h>
#include <stdio.h>

static int tests_run;
static int tests_failed;
static int math_assert_failures;

static void math_assert_failed(const char* expression)
{
	(void)expression;
	math_assert_failures++;
}

static void check_condition(int condition, const char* expression, const char* file, int line)
{
	tests_run++;
	if (!condition)
	{
		printf("FAIL %s:%d: %s\n", file, line, expression);
		tests_failed++;
	}
}

#define CHECK(condition) check_condition(!!(condition), #condition, __FILE__, __LINE__)

#define CHECK_CLOSE(actual, expected, tolerance)               \
	do                                                         \
	{                                                          \
		f64 check_actual = (f64)(actual);                      \
		f64 check_expected = (f64)(expected);                  \
		f64 check_delta = fabs(check_actual - check_expected); \
		CHECK(check_delta <= (f64)(tolerance));                \
	} while (0)

static void check_vec3(const rg_vec3* value, f32 x, f32 y, f32 z, f32 tolerance)
{
	CHECK_CLOSE(value->x, x, tolerance);
	CHECK_CLOSE(value->y, y, tolerance);
	CHECK_CLOSE(value->z, z, tolerance);
}

static void test_layout(void)
{
	CHECK(sizeof(rg_vec2) == sizeof(f32) * 2);
	CHECK(sizeof(rg_vec3) == sizeof(f32) * 4);
	CHECK(sizeof(rg_vec4) == sizeof(f32) * 4);
	CHECK(sizeof(rg_mat4) == sizeof(f32) * 16);
	CHECK(sizeof(rg_quat) == sizeof(f32) * 4);
	CHECK(RG_ALIGNOF(rg_vec3) >= 16);
	CHECK(RG_ALIGNOF(rg_vec4) >= 16);
	CHECK(RG_ALIGNOF(rg_mat4) >= 16);
}

static void test_scalar(void)
{
	CHECK_CLOSE(rg_rad(180.0f), RG_PI, 1e-6f);
	CHECK_CLOSE(rg_deg(RG_PI), 180.0f, 1e-5f);
	CHECK_CLOSE(rg_clamp(2.0f, 0.0f, 1.0f), 1.0f, 1e-7f);
	CHECK_CLOSE(rg_lerp(-2.0f, 6.0f, 0.25f), 0.0f, 1e-7f);
	CHECK_CLOSE(rg_smoothstep(0.0f, 1.0f, 0.5f), 0.5f, 1e-7f);
	CHECK_CLOSE(rg_sqrtf(81.0f), 9.0f, 2e-3f);
	CHECK_CLOSE(rg_sinf(RG_HALF_PI), 1.0f, 1e-5f);
	CHECK_CLOSE(rg_cosf(RG_PI), -1.0f, 1e-5f);
	const f32 angles[] = {-RG_PI, -RG_HALF_PI, -0.375f, 0.0f, 0.375f, RG_HALF_PI, RG_PI};
	for (size_t i = 0; i < RG_ARRAY_COUNT(angles); ++i)
	{
		f32 sine, cosine;
		rg_sincosf(angles[i], &sine, &cosine);
		CHECK_CLOSE(sine, sinf(angles[i]), 2e-5f);
		CHECK_CLOSE(cosine, cosf(angles[i]), 2e-5f);
	}
	CHECK(rg_imin(-4, 3) == -4);
	CHECK(rg_imax(-4, 3) == 3);

	// Rounding helpers must match libc bit for bit, including the sign of zero,
	// values just below one half, and integers beyond 2^23 where adding 0.5
	// before truncating misrounds.
	{
		static const f32 inputs[] = {0.0f, -0.0f, 0.49999997f, -0.49999997f, 0.5f, -0.5f, 1.5f, -1.5f, 2.5f, -2.5f,
		                             -0.3f, 0.3f, 8388609.0f, -8388609.0f, 16777217.0f, 1e30f, -1e30f, 3.0f, -7.0f};
		for (size_t i = 0; i < RG_ARRAY_COUNT(inputs); ++i)
		{
			rg_float_bits got, want;
			got.f = rg_roundf(inputs[i]);
			want.f = roundf(inputs[i]);
			CHECK(got.u == want.u);
			got.f = rg_floorf(inputs[i]);
			want.f = floorf(inputs[i]);
			CHECK(got.u == want.u);
			got.f = rg_ceilf(inputs[i]);
			want.f = ceilf(inputs[i]);
			CHECK(got.u == want.u);
		}
	}

	// Lane-0 operations must not evaluate unrelated lanes: the length of
	// (1, -1, 0) leaves -1 next to the squared length, and a square root of it
	// would raise the invalid-operation flag.
	{
		rg_vec3 v;
		rg_vec3_set(&v, 1.0f, -1.0f, 0.0f);
		feclearexcept(FE_INVALID);
		CHECK_CLOSE(rg_vec3_len(&v), 1.41421356f, 1e-6f);
		CHECK(fetestexcept(FE_INVALID) == 0);
	}
}

static void test_vectors(void)
{
	rg_vec3 a;
	rg_vec3 b;
	rg_vec3 out;
	rg_vec3_set(&a, 1.0f, 2.0f, 3.0f);
	rg_vec3_set(&b, 4.0f, -5.0f, 6.0f);

	rg_vec3_add(&a, &b, &out);
	check_vec3(&out, 5.0f, -3.0f, 9.0f, 1e-6f);
	rg_vec3_sub(&a, &b, &out);
	check_vec3(&out, -3.0f, 7.0f, -3.0f, 1e-6f);
	rg_vec3_scale(&a, 2.0f, &out);
	check_vec3(&out, 2.0f, 4.0f, 6.0f, 1e-6f);
	CHECK_CLOSE(rg_vec3_dot(&a, &b), 12.0f, 1e-5f);

	rg_vec3_cross(&a, &b, &out);
	check_vec3(&out, 27.0f, 6.0f, -13.0f, 1e-5f);
	CHECK_CLOSE(rg_vec3_dot(&a, &out), 0.0f, 1e-5f);
	CHECK_CLOSE(rg_vec3_dot(&b, &out), 0.0f, 1e-5f);

	rg_vec3_set(&a, 3.0f, 4.0f, 0.0f);
	CHECK_CLOSE(rg_vec3_len(&a), 5.0f, 2e-3f);
	rg_vec3_normalize(&a, &out);
	check_vec3(&out, 0.6f, 0.8f, 0.0f, 2e-3f);
}

static void test_vec4_floor_and_sqrt(void)
{
	const f32 floor_inputs[][4] = {
		{-1.25f, 2.75f, -3.0f, 0.5f},
		{-0.0f, 0.0f, -0.5f, 8388608.0f},
		{-8388608.0f, 1.9999999f, -1.0000001f, 123.456f}
	};
	const f32 sqrt_inputs[][4] = {
		{1.0f, 4.0f, 9.0f, 16.0f},
		{0.0f, 0.25f, 2.0f, 0.0001f},
		{1000000.0f, 0.5f, 25.0f, 0.000001f}
	};
	for (size_t i = 0; i < RG_ARRAY_COUNT(floor_inputs); ++i)
	{
		rg_vec4 input, output, in_place;
		rg_vec4_set(&input, floor_inputs[i][0], floor_inputs[i][1], floor_inputs[i][2], floor_inputs[i][3]);
		rg_vec4_floor(&input, &output);
		in_place = input;
		rg_vec4_floor(&in_place, &in_place);
		for (size_t lane = 0; lane < 4; ++lane)
		{
			rg_float_bits expected, actual, aliased;
			expected.f = rg_floorf(input.data[lane]);
			actual.f = output.data[lane];
			aliased.f = in_place.data[lane];
			CHECK(actual.u == expected.u);
			CHECK(aliased.u == expected.u);
		}
	}
	for (size_t i = 0; i < RG_ARRAY_COUNT(sqrt_inputs); ++i)
	{
		rg_vec4 input, output, in_place;
		rg_vec4_set(&input, sqrt_inputs[i][0], sqrt_inputs[i][1], sqrt_inputs[i][2], sqrt_inputs[i][3]);
		rg_vec4_sqrt(&input, &output);
		in_place = input;
		rg_vec4_sqrt(&in_place, &in_place);
		for (size_t lane = 0; lane < 4; ++lane)
		{
			f32 expected = sqrtf(input.data[lane]);
			CHECK_CLOSE(output.data[lane], expected, 2e-5f * (1.0f + expected));
			CHECK(output.data[lane] == in_place.data[lane]);
		}
	}
#if RG_MATH_USE_LIBC && RG_MATH_LIBC_ALIASES
	{
		rg_vec4 input = rg_vec4(-0.0f, INFINITY, -INFINITY, NAN);
		rg_vec4 output;
		rg_float_bits zero;
		rg_vec4_floor(&input, &output);
		zero.f = output.x;
		CHECK(zero.u == UINT32_C(0x80000000));
		CHECK(output.y == INFINITY);
		CHECK(output.z == -INFINITY);
		CHECK(isnan(output.w));
		rg_vec4_sqrt(&input, &output);
		zero.f = output.x;
		CHECK(zero.u == UINT32_C(0x80000000));
		CHECK(output.y == INFINITY);
		CHECK(isnan(output.z));
		CHECK(isnan(output.w));
	}
#endif
}

static void test_matrices(void)
{
	rg_mat4 identity;
	rg_mat4 translation;
	rg_mat4 inverse;
	rg_mat4 product;
	rg_vec3 offset;
	rg_vec3 point;
	rg_vec3 transformed;

	rg_mat4_identity(&identity);
	for (i32 column = 0; column < 4; column++)
	{
		for (i32 row = 0; row < 4; row++)
		{
			f32 expected = column == row ? 1.0f : 0.0f;
			CHECK_CLOSE(identity.m[column * 4 + row], expected, 1e-7f);
		}
	}

	rg_vec3_set(&offset, 5.0f, -2.0f, 3.0f);
	rg_mat4_translate_make(&translation, &offset);
	rg_vec3_set(&point, 1.0f, 2.0f, 3.0f);
	rg_mat4_mulv3(&translation, &point, 1.0f, &transformed);
	check_vec3(&transformed, 6.0f, 0.0f, 6.0f, 1e-5f);

	rg_mat4_inv(&translation, &inverse);
	rg_mat4_mul(&translation, &inverse, &product);
	for (i32 column = 0; column < 4; column++)
	{
		for (i32 row = 0; row < 4; row++)
		{
			f32 expected = column == row ? 1.0f : 0.0f;
			CHECK_CLOSE(product.m[column * 4 + row], expected, 2e-4f);
		}
	}
	CHECK_CLOSE(rg_mat4_det(&translation), 1.0f, 1e-5f);
}

static void test_quaternions_and_euler(void)
{
	rg_vec3 axis;
	rg_vec3 x_axis;
	rg_vec3 rotated;
	rg_vec3 angles;
	rg_quat rotation;
	rg_mat4 matrix;

	rg_vec3_set(&axis, 0.0f, 0.0f, 1.0f);
	rg_quat_from_axis_angle(RG_HALF_PI, &axis, &rotation);
	rg_vec3_set(&x_axis, 1.0f, 0.0f, 0.0f);
	rg_quat_rotatev(&rotation, &x_axis, &rotated);
	check_vec3(&rotated, 0.0f, 1.0f, 0.0f, 2e-3f);

	rg_vec3_set(&angles, 0.0f, 0.0f, 0.0f);
	rg_mat4_euler_xyz(&angles, &matrix);
	for (i32 i = 0; i < 16; i++)
	{
		f32 expected = (i % 5) == 0 ? 1.0f : 0.0f;
		CHECK_CLOSE(matrix.m[i], expected, 1e-6f);
	}
}

static void test_geometry(void)
{
	rg_vec3 origin;
	rg_vec3 direction;
	rg_vec3 center;
	rg_vec3 hit;
	rg_ray ray;
	rg_sphere sphere;
	f32 near_t = 0.0f;
	f32 far_t = 0.0f;

	rg_vec3_set(&origin, 0.0f, 0.0f, -5.0f);
	rg_vec3_set(&direction, 0.0f, 0.0f, 1.0f);
	rg_ray_set(&ray, &origin, &direction);
	rg_vec3_set(&center, 0.0f, 0.0f, 0.0f);
	rg_sphere_set(&sphere, &center, 1.0f);
	CHECK(rg_ray_sphere(&ray, &sphere, &near_t, &far_t));
	CHECK_CLOSE(near_t, 4.0f, 2e-3f);
	CHECK_CLOSE(far_t, 6.0f, 2e-3f);
	rg_ray_at(&ray, near_t, &hit);
	check_vec3(&hit, 0.0f, 0.0f, -1.0f, 2e-3f);
}

static void test_ray_sphere_roots(void)
{
	rg_sphere sphere;
	rg_vec3 center = rg_vec3(0.0f, 0.0f, 0.0f);
	rg_ray ray;
	f32 near_t, far_t;
	rg_sphere_set(&sphere, &center, 1.0f);
	rg_vec3_set(&ray.dir, 0.0f, 1.0f, 0.0f);

	// A tangent ray starting on the sphere has a repeated zero root.
	rg_vec3_set(&ray.origin, 1.0f, 0.0f, 0.0f);
	CHECK(rg_ray_sphere(&ray, &sphere, &near_t, &far_t));
	CHECK(near_t == 0.0f);
	CHECK(far_t == 0.0f);
	CHECK(rg_ray_sphere(&ray, &sphere, NULL, NULL));
	CHECK(rg_ray_sphere(&ray, &sphere, NULL, &far_t));
	CHECK(far_t == 0.0f);
	CHECK(rg_ray_sphere(&ray, &sphere, &near_t, NULL));
	CHECK(near_t == 0.0f);

	// Nonzero repeated roots retain their distance and forward-hit semantics.
	rg_vec3_set(&ray.origin, 1.0f, -2.0f, 0.0f);
	CHECK(rg_ray_sphere(&ray, &sphere, &near_t, &far_t));
	CHECK_CLOSE(near_t, 2.0f, 2e-5f);
	CHECK_CLOSE(far_t, 2.0f, 2e-5f);
	rg_vec3_set(&ray.origin, 1.0f, 2.0f, 0.0f);
	CHECK(!rg_ray_sphere(&ray, &sphere, &near_t, &far_t));
	CHECK_CLOSE(near_t, -2.0f, 2e-5f);
	CHECK_CLOSE(far_t, -2.0f, 2e-5f);

	rg_vec3_set(&ray.origin, 0.0f, 0.0f, 0.0f);
	CHECK(rg_ray_sphere(&ray, &sphere, &near_t, &far_t));
	CHECK_CLOSE(near_t, -1.0f, 2e-5f);
	CHECK_CLOSE(far_t, 1.0f, 2e-5f);
	rg_vec3_set(&ray.origin, 0.0f, 1.0f, 0.0f);
	CHECK(rg_ray_sphere(&ray, &sphere, &near_t, &far_t));
	CHECK_CLOSE(near_t, -2.0f, 2e-5f);
	CHECK(far_t == 0.0f);
	rg_vec3_set(&ray.origin, 2.0f, 0.0f, 0.0f);
	near_t = far_t = 123.0f;
	CHECK(!rg_ray_sphere(&ray, &sphere, &near_t, &far_t));
	CHECK(near_t == 123.0f && far_t == 123.0f);
}

static void test_auxiliary_modules(void)
{
	rg_vec3 color;
	rg_vec2 noise_point;
	rg_mat4 projection;

	rg_vec3_set(&color, 1.0f, 1.0f, 1.0f);
	CHECK_CLOSE(rg_luminance(&color), 1.0f, 1e-6f);
	CHECK_CLOSE(rg_bezier(0.5f, 0.0f, 0.0f, 1.0f, 1.0f), 0.5f, 1e-6f);

	rg_vec2_set(&noise_point, 2.0f, -3.0f);
	CHECK_CLOSE(rg_perlin_vec2(&noise_point), 0.0f, 1e-6f);

	rg_mat4_perspective_default(16.0f / 9.0f, &projection);
	CHECK(projection.m[0] > 0.0f);
	CHECK(projection.m[5] > 0.0f);
	CHECK(projection.m[11] != 0.0f);
}

static void check_mat4(const rg_mat4* actual, const rg_mat4* expected, f32 tolerance)
{
	for (i32 i = 0; i < 16; ++i)
		CHECK_CLOSE(actual->m[i], expected->m[i], tolerance);
}

// An independent column-major reference also covers non-affine inputs.
static void reference_mul(const rg_mat4* a, const rg_mat4* b, rg_mat4* out)
{
	rg_mat4 result;
	for (i32 col = 0; col < 4; ++col)
	{
		for (i32 row = 0; row < 4; ++row)
		{
			f64 sum = 0.0;
			for (i32 k = 0; k < 4; ++k)
				sum += (f64)a->m[k * 4 + row] * b->m[col * 4 + k];
			result.m[col * 4 + row] = (f32)sum;
		}
	}
	*out = result;
}

static void test_rotation_round_trips(void)
{
	const f32 axes[][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 2, -3}};
	// Exercise positive trace and each largest-component extraction branch.
	const f32 angles[] = {0.0f, 0.7f, RG_HALF_PI, 2.7f, RG_PI, -2.7f};
	const f32 scales[][3] = {{1, 1, 1}, {2, 3, 0.5f}, {-2, 3, 0.5f}};
	for (size_t a = 0; a < RG_ARRAY_COUNT(axes); ++a)
	{
		rg_vec3 axis;
		f32 inv_len = 1.0f / sqrtf(axes[a][0] * axes[a][0] + axes[a][1] * axes[a][1] + axes[a][2] * axes[a][2]);
		rg_vec3_set(&axis, axes[a][0] * inv_len, axes[a][1] * inv_len, axes[a][2] * inv_len);
		for (size_t n = 0; n < RG_ARRAY_COUNT(angles); ++n)
		{
			rg_quat q, extracted;
			rg_mat4 matrix, reconstructed;
			rg_mat3 matrix3, reconstructed3;
			rg_quat_from_axis_angle(angles[n], &axis, &q);
			rg_quat_mat4(&q, &matrix);
			rg_quat_from_mat4(&matrix, &extracted);
			rg_quat_mat4(&extracted, &reconstructed);
			check_mat4(&reconstructed, &matrix, 3e-4f);
			rg_mat4_quat(&matrix, &extracted);
			rg_quat_mat4(&extracted, &reconstructed);
			check_mat4(&reconstructed, &matrix, 3e-4f);
			rg_quat_mat3(&q, &matrix3);
			rg_quat_from_mat3(&matrix3, &extracted);
			rg_quat_mat3(&extracted, &reconstructed3);
			for (i32 col = 0; col < 3; ++col)
				for (i32 row = 0; row < 3; ++row)
					CHECK_CLOSE(reconstructed3.m[col * 4 + row], matrix3.m[col * 4 + row], 3e-4f);

			for (size_t k = 0; k < RG_ARRAY_COUNT(scales); ++k)
			{
				rg_vec3 translation = rg_vec3(3.0f, -5.0f, 7.0f);
				rg_vec3 scale = rg_vec3(scales[k][0], scales[k][1], scales[k][2]);
				rg_vec3 out_t, out_s;
				rg_mat4_trs_make(&matrix, &translation, &q, &scale);
				rg_mat4_decompose(&matrix, &out_t, &extracted, &out_s);
				rg_mat4_trs_make(&reconstructed, &out_t, &extracted, &out_s);
				check_mat4(&reconstructed, &matrix, 1e-3f);
				rg_mat4_decompose_fast(&matrix, &out_t, &extracted, &out_s);
				rg_mat4_trs_make(&reconstructed, &out_t, &extracted, &out_s);
				check_mat4(&reconstructed, &matrix, 8e-3f);
			}
		}
	}
}

static void check_view(const rg_mat4* view, const rg_vec3* eye, const rg_vec3* target, int left_handed)
{
	rg_vec3 transformed;
	rg_mat4_mulv3(view, eye, 1.0f, &transformed);
	check_vec3(&transformed, 0.0f, 0.0f, 0.0f, 2e-5f);
	f32 dx = target->x - eye->x;
	f32 dy = target->y - eye->y;
	f32 dz = target->z - eye->z;
	f32 distance = sqrtf(dx * dx + dy * dy + dz * dz);
	rg_mat4_mulv3(view, target, 1.0f, &transformed);
#if RG_MATH_MAX_PERF && !defined(RG_MATH_SIMD)
	// The explicit scalar fast rsqrt uses an unrefined approximation.
	f32 tolerance = 0.05f * distance;
#else
	f32 tolerance = 0.002f * distance;
#endif
	check_vec3(&transformed, 0.0f, 0.0f, left_handed ? distance : -distance, tolerance);
}

static void test_camera_basis(void)
{
	const f32 targets[][3] = {{4, 2, 3}, {-2, 4, -5}, {1.01f, 5, 3.01f}};
	rg_vec3 eye = rg_vec3(1, 2, 3);
	rg_vec3 up = rg_vec3(0, 1, 0);
	for (size_t i = 0; i < RG_ARRAY_COUNT(targets); ++i)
	{
		rg_vec3 target = rg_vec3(targets[i][0], targets[i][1], targets[i][2]);
		rg_vec3 dir;
		rg_mat4 view, expected;
		rg_vec3_sub(&target, &eye, &dir);
		rg_mat4_look_at_rh(&eye, &target, &up, &view);
		check_view(&view, &eye, &target, 0);
		rg_mat4_look_at_lh(&eye, &target, &up, &view);
		check_view(&view, &eye, &target, 1);
		rg_mat4_look_rh(&eye, &dir, &up, &view);
		check_view(&view, &eye, &target, 0);
		rg_mat4_look_lh(&eye, &dir, &up, &view);
		check_view(&view, &eye, &target, 1);
		rg_mat4_look_anyup_rh(&eye, &dir, &view);
		check_view(&view, &eye, &target, 0);
		rg_mat4_look_anyup_lh(&eye, &dir, &view);
		check_view(&view, &eye, &target, 1);
		rg_mat4_look_at(&eye, &target, &up, &view);
#if (RG_MATH_CLIP_CONTROL & RG_MATH_CLIP_CONTROL_LH_BIT)
		rg_mat4_look_at_lh(&eye, &target, &up, &expected);
#else
		rg_mat4_look_at_rh(&eye, &target, &up, &expected);
#endif
		check_mat4(&view, &expected, 1e-6f);
	}
}

typedef void (*MakeFrustum)(f32, f32, f32, f32, f32, f32, rg_mat4*);
typedef void (*DecomposeFrustum)(const rg_mat4*, f32*, f32*, f32*, f32*, f32*, f32*);
typedef void (*DecomposeAxis)(const rg_mat4*, f32*, f32*);

static void test_projection_and_frustum(void)
{
	const MakeFrustum make[] = {rg_mat4_frustum_rh_no, rg_mat4_frustum_rh_zo, rg_mat4_frustum_lh_no, rg_mat4_frustum_lh_zo};
	const MakeFrustum ortho[] = {rg_mat4_ortho_rh_no, rg_mat4_ortho_rh_zo, rg_mat4_ortho_lh_no, rg_mat4_ortho_lh_zo};
	const DecomposeFrustum decompose[] = {rg_mat4_persp_decomp_rh_no, rg_mat4_persp_decomp_rh_zo, rg_mat4_persp_decomp_lh_no, rg_mat4_persp_decomp_lh_zo};
	const DecomposeAxis decompose_x[] = {rg_mat4_persp_decomp_x_rh_no, rg_mat4_persp_decomp_x_rh_zo, rg_mat4_persp_decomp_x_lh_no, rg_mat4_persp_decomp_x_lh_zo};
	const DecomposeAxis decompose_y[] = {rg_mat4_persp_decomp_y_rh_no, rg_mat4_persp_decomp_y_rh_zo, rg_mat4_persp_decomp_y_lh_no, rg_mat4_persp_decomp_y_lh_zo};
	for (i32 mode = 0; mode < 4; ++mode)
	{
		int zo = mode & 1;
		f32 sign = mode >= 2 ? 1.0f : -1.0f;
		for (i32 orthographic = 0; orthographic < 2; ++orthographic)
		{
			rg_mat4 p, inv;
			rg_frustum frustum;
			(orthographic ? ortho[mode] : make[mode])(-0.75f, 2.25f, -1.25f, 0.5f, 1.1f, 37.0f, &p);
			if (!orthographic)
			{
				f32 n, f, top, bottom, left, right;
				decompose[mode](&p, &n, &f, &top, &bottom, &left, &right);
				CHECK_CLOSE(n, 1.1f, 1e-5f);
				CHECK_CLOSE(f, 37.0f, 2e-3f);
				CHECK_CLOSE(left, -0.75f, 1e-5f);
				CHECK_CLOSE(right, 2.25f, 1e-5f);
				CHECK_CLOSE(bottom, -1.25f, 1e-5f);
				CHECK_CLOSE(top, 0.5f, 1e-5f);
				decompose_x[mode](&p, &left, &right);
				decompose_y[mode](&p, &top, &bottom);
				CHECK_CLOSE(left, -0.75f, 1e-5f);
				CHECK_CLOSE(right, 2.25f, 1e-5f);
				CHECK_CLOSE(bottom, -1.25f, 1e-5f);
				CHECK_CLOSE(top, 0.5f, 1e-5f);
			}
			// Canonical NDC corners prevent matching constructor/decomposer errors.
			for (i32 far_corner = 0; far_corner < 2; ++far_corner)
			{
				f32 depth = far_corner ? 37.0f : 1.1f;
				f32 scale = orthographic ? 1.0f : depth / 1.1f;
				for (i32 corner = 0; corner < 4; ++corner)
				{
					rg_vec4 v = rg_vec4((corner & 1 ? 2.25f : -0.75f) * scale, (corner & 2 ? 0.5f : -1.25f) * scale, sign * depth, 1.0f);
					rg_vec4 clip;
					rg_mat4_mulv4(&p, &v, &clip);
					CHECK_CLOSE(clip.x / clip.w, corner & 1 ? 1.0f : -1.0f, 2e-5f);
					CHECK_CLOSE(clip.y / clip.w, corner & 2 ? 1.0f : -1.0f, 2e-5f);
					CHECK_CLOSE(clip.z / clip.w, far_corner ? 1.0f : (zo ? 0.0f : -1.0f), 2e-5f);
				}
			}
			if (zo) rg_frustum_from_mat4_zo(&p, &frustum);
			else rg_frustum_from_mat4_no(&p, &frustum);
			rg_mat4_inv(&p, &inv);
			for (i32 side = -1; side < RG_FRUSTUM_PLANE_COUNT; ++side)
			{
				rg_vec4 clip = rg_vec4(0, 0, zo ? 0.5f : 0.0f, 1);
				rg_vec4 world;
				rg_aabb box;
				if (side >= 0)
				{
					i32 component = side / 2;
					clip.data[component] = side & 1 ? 1.1f : (component == 2 && zo ? -0.1f : -1.1f);
					// Stay on the forward side of the perspective asymptote.
					if (side == RG_FRUSTUM_FAR) clip.z = 1.01f;
				}
				rg_mat4_mulv4(&inv, &clip, &world);
				rg_vec3_set(&box.min, world.x / world.w - 0.001f, world.y / world.w - 0.001f, world.z / world.w - 0.001f);
				rg_vec3_set(&box.max, world.x / world.w + 0.001f, world.y / world.w + 0.001f, world.z / world.w + 0.001f);
				CHECK(rg_aabb_in_frustum(&box, &frustum) == (side < 0));
			}
		}
	}
	// Verify the configured entrypoint independently of the explicit variants.
	{
		rg_mat4 p;
		rg_frustum actual, expected;
		rg_mat4_perspective(RG_HALF_PI, 1.5f, 1.0f, 10.0f, &p);
		rg_frustum_from_mat4(&p, &actual);
#if (RG_MATH_CLIP_CONTROL & RG_MATH_CLIP_CONTROL_ZO_BIT)
		rg_frustum_from_mat4_zo(&p, &expected);
#else
		rg_frustum_from_mat4_no(&p, &expected);
#endif
		for (i32 i = 0; i < RG_FRUSTUM_PLANE_COUNT; ++i)
		{
			check_vec3(&actual.planes[i].normal, expected.planes[i].normal.x, expected.planes[i].normal.y, expected.planes[i].normal.z, 1e-6f);
			CHECK_CLOSE(actual.planes[i].d, expected.planes[i].d, 1e-6f);
		}
	}
}

static void test_inverse_and_multiply_contracts(void)
{
	rg_vec3 axis = rg_vec3(0, 1, 0), translation = rg_vec3(3, -5, 7), scale = rg_vec3(1, 1, 1);
	rg_quat q;
	rg_mat4 m, inverse, identity, product, in_place;
	rg_quat_from_axis_angle(0.7f, &axis, &q);
	rg_mat4_trs_make(&m, &translation, &q, &scale);
	rg_mat4_identity(&identity);
	rg_mat4_inv_tr(&m, &inverse);
	reference_mul(&m, &inverse, &product);
	check_mat4(&product, &identity, 3e-4f);
	in_place = m;
	rg_mat4_inv_tr(&in_place, &in_place);
	check_mat4(&in_place, &inverse, 1e-6f);
	rg_vec3_set(&scale, 2, 3, 0.5f);
	rg_mat4_trs_make(&m, &translation, &q, &scale);
	m.m[4] += 0.3f; // A nonsingular affine matrix with shear.
	rg_mat4_inv_affine(&m, &inverse);
	reference_mul(&m, &inverse, &product);
	check_mat4(&product, &identity, 3e-4f);
	in_place = m;
	rg_mat4_inv_affine(&in_place, &in_place);
	check_mat4(&in_place, &inverse, 1e-6f);
	{
		RG_ALIGN32 rg_mat4 aligned[5];
		// +16 deliberately satisfies rg_mat4 alignment but not the fast contract.
		struct OffsetMatrices { f32 prefix[4]; rg_mat4 values[5]; };
		RG_ALIGN32 struct OffsetMatrices storage;
		rg_mat4* unaligned32 = storage.values;
		rg_mat4 expected, tmp;
		CHECK(((uintptr_t)unaligned32 & 31u) == 16u);
		for (i32 i = 0; i < 4; ++i)
		{
			for (i32 j = 0; j < 16; ++j)
				aligned[i].m[j] = (f32)((j * 7 + i * 3) % 13 - 6) * 0.125f;
			unaligned32[i] = aligned[i];
		}
		reference_mul(&aligned[0], &aligned[1], &tmp);
		reference_mul(&tmp, &aligned[2], &expected);
		reference_mul(&expected, &aligned[3], &tmp);
		expected = tmp;
		rg_mat4_mul4_aligned32(&aligned[0], &aligned[1], &aligned[2], &aligned[3], &aligned[4]);
		check_mat4(&aligned[4], &expected, 2e-5f);
		rg_mat4_mul4(&unaligned32[0], &unaligned32[1], &unaligned32[2], &unaligned32[3], &unaligned32[4]);
		check_mat4(&unaligned32[4], &expected, 2e-5f);
		for (i32 alias = 0; alias < 4; ++alias)
		{
			RG_ALIGN32 rg_mat4 copy[4];
			for (i32 i = 0; i < 4; ++i) copy[i] = aligned[i];
			rg_mat4_mul4_aligned32(&copy[0], &copy[1], &copy[2], &copy[3], &copy[alias]);
			check_mat4(&copy[alias], &expected, 2e-5f);
			for (i32 i = 0; i < 4; ++i) unaligned32[i] = aligned[i];
			rg_mat4_mul4(&unaligned32[0], &unaligned32[1], &unaligned32[2], &unaligned32[3], &unaligned32[alias]);
			check_mat4(&unaligned32[alias], &expected, 2e-5f);
		}
	}
}


// =============================================================================
// Geometry fast paths, batched culling, ray_inv, slerp, and regressions
// =============================================================================

static uint32_t test_rng_state = UINT32_C(0x9E3779B9);

static f32 test_random_unit(void)
{
	test_rng_state ^= test_rng_state << 13;
	test_rng_state ^= test_rng_state >> 17;
	test_rng_state ^= test_rng_state << 5;
	return (f32)(test_rng_state >> 8) * (1.0f / 16777216.0f);
}

static f32 test_random_range(f32 lo, f32 hi)
{
	return lo + (hi - lo) * test_random_unit();
}

static void test_random_box(rg_aabb* box, f32 center_range, f32 max_extent)
{
	rg_vec3 center = rg_vec3(test_random_range(-center_range, center_range), test_random_range(-center_range, center_range), test_random_range(-center_range, center_range));
	rg_vec3 extents = rg_vec3(test_random_range(0.0f, max_extent), test_random_range(0.0f, max_extent), test_random_range(0.0f, max_extent));
	rg_aabb_from_center_extents(box, &center, &extents);
}

static void test_ray_aabb(void)
{
	rg_aabb box;
	rg_ray ray;
	rg_vec3 min = rg_vec3(-1, -1, -1), max = rg_vec3(1, 1, 1);
	f32 tmin, tmax;
	rg_aabb_set(&box, &min, &max);

	// Straight hit from the front.
	rg_vec3_set(&ray.origin, 0, 0, -5);
	rg_vec3_set(&ray.dir, 0, 0, 1);
	tmin = tmax = 123.0f;
	CHECK(rg_ray_aabb(&ray, &box, &tmin, &tmax));
	CHECK_CLOSE(tmin, 4.0f, 1e-5f);
	CHECK_CLOSE(tmax, 6.0f, 1e-5f);
	CHECK(rg_ray_aabb(&ray, &box, NULL, NULL));
	// Negative direction component.
	rg_vec3_set(&ray.origin, 0, 0, 5);
	rg_vec3_set(&ray.dir, 0, 0, -1);
	CHECK(rg_ray_aabb(&ray, &box, &tmin, &tmax));
	CHECK_CLOSE(tmin, 4.0f, 1e-5f);
	CHECK_CLOSE(tmax, 6.0f, 1e-5f);
	// Non-normalized direction scales the distances.
	rg_vec3_set(&ray.origin, 0, 0, -5);
	rg_vec3_set(&ray.dir, 0, 0, 2);
	CHECK(rg_ray_aabb(&ray, &box, &tmin, &tmax));
	CHECK_CLOSE(tmin, 2.0f, 1e-5f);
	CHECK_CLOSE(tmax, 3.0f, 1e-5f);
	// Origin inside: negative near distance.
	rg_vec3_set(&ray.origin, 0.5f, 0, 0);
	rg_vec3_set(&ray.dir, 1, 0, 0);
	CHECK(rg_ray_aabb(&ray, &box, &tmin, &tmax));
	CHECK_CLOSE(tmin, -1.5f, 1e-5f);
	CHECK_CLOSE(tmax, 0.5f, 1e-5f);
	// Box behind the origin: a miss that still reports the overlapping slabs.
	rg_vec3_set(&ray.origin, 0, 0, -5);
	rg_vec3_set(&ray.dir, 0, 0, -1);
	tmin = tmax = 123.0f;
	CHECK(!rg_ray_aabb(&ray, &box, &tmin, &tmax));
	CHECK_CLOSE(tmin, -6.0f, 1e-5f);
	CHECK_CLOSE(tmax, -4.0f, 1e-5f);
	// Slab miss leaves the outputs untouched.
	rg_vec3_set(&ray.origin, 5, 0, -5);
	rg_vec3_set(&ray.dir, 0, 0, 1);
	tmin = tmax = 123.0f;
	CHECK(!rg_ray_aabb(&ray, &box, &tmin, &tmax));
	CHECK(tmin == 123.0f && tmax == 123.0f);
	// Parallel to the x and y slabs with the origin inside them.
	rg_vec3_set(&ray.origin, 0.5f, -0.5f, -5);
	rg_vec3_set(&ray.dir, 0, 0, 1);
	CHECK(rg_ray_aabb(&ray, &box, &tmin, &tmax));
	CHECK_CLOSE(tmin, 4.0f, 1e-5f);
	CHECK_CLOSE(tmax, 6.0f, 1e-5f);
	// Parallel with the origin outside the slab.
	rg_vec3_set(&ray.origin, 2.0f, 0, -5);
	CHECK(!rg_ray_aabb(&ray, &box, &tmin, &tmax));
	rg_vec3_set(&ray.origin, 0, -1.0001f, -5);
	CHECK(!rg_ray_aabb(&ray, &box, &tmin, &tmax));
	// Parallel with the origin exactly on a face counts as touching, for +0 and -0.
	rg_vec3_set(&ray.origin, 1.0f, -1.0f, -5);
	rg_vec3_set(&ray.dir, 0.0f, -0.0f, 1);
	CHECK(rg_ray_aabb(&ray, &box, &tmin, &tmax));
	CHECK_CLOSE(tmin, 4.0f, 1e-5f);
	CHECK_CLOSE(tmax, 6.0f, 1e-5f);
	rg_vec3_set(&ray.dir, -0.0f, 0.0f, 1);
	CHECK(rg_ray_aabb(&ray, &box, &tmin, &tmax));
	CHECK_CLOSE(tmin, 4.0f, 1e-5f);
	CHECK_CLOSE(tmax, 6.0f, 1e-5f);
	// Direction below RG_EPSILON but nonzero is a real (very long) slab.
	rg_vec3_set(&ray.origin, 0, 0, -5);
	rg_vec3_set(&ray.dir, 1e-8f, 0, 1);
	CHECK(rg_ray_aabb(&ray, &box, &tmin, &tmax));
	CHECK_CLOSE(tmin, 4.0f, 1e-5f);
	// Zero direction: hit only when the origin is inside.
	rg_vec3_set(&ray.dir, 0, 0, 0);
	rg_vec3_set(&ray.origin, 0.25f, 0.25f, 0.25f);
	CHECK(rg_ray_aabb(&ray, &box, &tmin, &tmax));
	CHECK(tmin <= 0.0f && tmax >= 0.0f);
	rg_vec3_set(&ray.origin, 0.25f, 0.25f, 1.25f);
	CHECK(!rg_ray_aabb(&ray, &box, &tmin, &tmax));
	// Diagonal ray.
	{
		f32 s = 1.0f / sqrtf(3.0f);
		rg_vec3_set(&ray.origin, -5, -5, -5);
		rg_vec3_set(&ray.dir, s, s, s);
		CHECK(rg_ray_aabb(&ray, &box, &tmin, &tmax));
		CHECK_CLOSE(tmin, 4.0f * sqrtf(3.0f), 1e-4f);
		CHECK_CLOSE(tmax, 6.0f * sqrtf(3.0f), 1e-4f);
	}
	// Degenerate zero-extent box.
	rg_vec3_set(&min, 0, 0, 0);
	rg_aabb_set(&box, &min, &min);
	rg_vec3_set(&ray.origin, 0, 0, -5);
	rg_vec3_set(&ray.dir, 0, 0, 1);
	CHECK(rg_ray_aabb(&ray, &box, &tmin, &tmax));
	CHECK_CLOSE(tmin, 5.0f, 1e-5f);
	CHECK_CLOSE(tmax, 5.0f, 1e-5f);
	rg_vec3_set(&ray.origin, 0.1f, 0, -5);
	CHECK(!rg_ray_aabb(&ray, &box, &tmin, &tmax));
}

static void test_ray_inv_aabb(void)
{
	enum { BOX_COUNT = 37 };
	rg_aabb boxes[BOX_COUNT];
	f32 tmins[BOX_COUNT];
	u8 flags[BOX_COUNT];
	rg_ray ray;
	rg_ray_inv inv;
	rg_vec3 min = rg_vec3(-1, -1, -1), max = rg_vec3(1, 1, 1);
	f32 tmin, tmax, near_t, far_t;

	rg_aabb_set(&boxes[0], &min, &max);
	rg_vec3_set(&ray.origin, 0, 0, -5);
	rg_vec3_set(&ray.dir, 0, 0, 1);
	rg_ray_inv_make(&inv, &ray);
	check_vec3(&inv.origin, 0, 0, -5, 0.0f);
	check_vec3(&inv.dir, 0, 0, 1, 0.0f);
	CHECK(inv.inv_dir.z == 1.0f);
	CHECK(inv.inv_dir.x > 1e29f && inv.inv_dir.y > 1e29f); // clamped, not inf
	CHECK(rg_ray_inv_aabb(&inv, &boxes[0], &tmin, &tmax));
	CHECK_CLOSE(tmin, 4.0f, 1e-5f);
	CHECK_CLOSE(tmax, 6.0f, 1e-5f);
	rg_vec3_set(&ray.dir, -0.0f, 1e-9f, -3.0f);
	rg_ray_inv_make(&inv, &ray);
	CHECK(inv.inv_dir.x < -1e29f);
	CHECK(inv.inv_dir.y == 1e9f);
	CHECK(inv.inv_dir.z == 1.0f / -3.0f);
	// A slab miss still writes the (empty) interval.
	rg_vec3_set(&ray.origin, 5, 0, -5);
	rg_vec3_set(&ray.dir, -0.01f, 0, 1);
	rg_ray_inv_make(&inv, &ray);
	tmin = tmax = 123.0f;
	CHECK(!rg_ray_inv_aabb(&inv, &boxes[0], &tmin, &tmax));
	CHECK(tmin != 123.0f && tmax != 123.0f && tmin > tmax);
	CHECK(!rg_ray_inv_aabb(&inv, &boxes[0], NULL, NULL));
	// A parallel lane outside its slab is a miss too; the interval is then unconstrained in that lane.
	rg_vec3_set(&ray.dir, 0, 0, 1);
	rg_ray_inv_make(&inv, &ray);
	CHECK(!rg_ray_inv_aabb(&inv, &boxes[0], &tmin, &tmax));

	// Random rays agree with rg_ray_aabb, and the batch agrees with the single test.
	for (i32 iteration = 0; iteration < 200; ++iteration)
	{
		for (i32 k = 0; k < BOX_COUNT; ++k)
			test_random_box(&boxes[k], 10.0f, 3.0f);
		rg_aabb_center(&boxes[iteration % BOX_COUNT], &ray.origin);
		if (iteration & 1)
			rg_vec3_set(&ray.origin, test_random_range(-15, 15), test_random_range(-15, 15), test_random_range(-15, 15));
		rg_vec3_set(&ray.dir, test_random_range(-1, 1), test_random_range(-1, 1), test_random_range(-1, 1));
		if ((iteration & 7) == 2) ray.dir.data[iteration % 3] = 0.0f;
		if ((iteration & 7) == 3) ray.dir.data[iteration % 3] = -0.0f;
		rg_ray_inv_make(&inv, &ray);
		size_t count = (size_t)(iteration % 3 == 0 ? BOX_COUNT : (iteration % 3 == 1 ? 8 : 1));
		size_t hits = rg_ray_inv_aabbs(&inv, boxes, count, tmins, flags);
		size_t expected = 0;
		for (size_t k = 0; k < count; ++k)
		{
			int hit = rg_ray_inv_aabb(&inv, &boxes[k], &near_t, &far_t);
			int reference = rg_ray_aabb(&ray, &boxes[k], &tmin, &tmax);
			CHECK(hit == reference);
			if (hit && reference)
			{
				CHECK(near_t == tmin);
				CHECK(far_t == tmax);
			}
			CHECK(flags[k] == (u8)hit);
			CHECK(tmins[k] == near_t);
			expected += (size_t)hit;
		}
		CHECK(hits == expected);
	}
	// Count 0 touches nothing; tmins may be NULL.
	flags[0] = 7;
	tmins[0] = 123.0f;
	CHECK(rg_ray_inv_aabbs(&inv, boxes, 0, tmins, flags) == 0);
	CHECK(flags[0] == 7 && tmins[0] == 123.0f);
	{
		size_t hits = rg_ray_inv_aabbs(&inv, boxes, 5, NULL, flags);
		CHECK(hits == (size_t)flags[0] + flags[1] + flags[2] + flags[3] + flags[4]);
	}
}

static void test_ray_triangle(void)
{
	rg_vec3 a = rg_vec3(0, 0, 0), b = rg_vec3(1, 0, 0), c = rg_vec3(0, 1, 0);
	rg_ray ray;
	f32 t;

	rg_vec3_set(&ray.origin, 0.25f, 0.25f, -1);
	rg_vec3_set(&ray.dir, 0, 0, 1);
	t = 0.0f;
	CHECK(rg_ray_triangle(&ray, &a, &b, &c, &t));
	CHECK_CLOSE(t, 1.0f, 1e-6f);
	// Reversed winding (negative determinant) hits as well.
	CHECK(rg_ray_triangle(&ray, &a, &c, &b, &t));
	CHECK_CLOSE(t, 1.0f, 1e-6f);
	CHECK(rg_ray_triangle(&ray, &a, &b, &c, NULL));
	// Triangle behind the origin: t is written but the result is a miss.
	rg_vec3_set(&ray.origin, 0.25f, 0.25f, 1);
	CHECK(!rg_ray_triangle(&ray, &a, &b, &c, &t));
	CHECK_CLOSE(t, -1.0f, 1e-6f);
	// Outside the hypotenuse edge, and outside the u/v edges.
	rg_vec3_set(&ray.origin, 0.75f, 0.75f, -1);
	CHECK(!rg_ray_triangle(&ray, &a, &b, &c, &t));
	rg_vec3_set(&ray.origin, -0.01f, 0.25f, -1);
	CHECK(!rg_ray_triangle(&ray, &a, &b, &c, &t));
	rg_vec3_set(&ray.origin, 0.25f, -0.01f, -1);
	CHECK(!rg_ray_triangle(&ray, &a, &b, &c, &t));
	// Edge-on: direction in the triangle plane.
	rg_vec3_set(&ray.origin, -1, 0.25f, 0);
	rg_vec3_set(&ray.dir, 1, 0, 0);
	CHECK(!rg_ray_triangle(&ray, &a, &b, &c, &t));
	// Vertex hits (u or v at the boundary) are inclusive.
	rg_vec3_set(&ray.origin, 1, 0, -2);
	rg_vec3_set(&ray.dir, 0, 0, 1);
	CHECK(rg_ray_triangle(&ray, &a, &b, &c, &t));
	CHECK_CLOSE(t, 2.0f, 1e-6f);

	// Random cases agree with the barycentric variant (independent arithmetic).
	for (i32 i = 0; i < 2000; ++i)
	{
		rg_vec3 v0 = rg_vec3(test_random_range(-4, 4), test_random_range(-4, 4), test_random_range(-4, 4));
		rg_vec3 v1 = rg_vec3(v0.x + test_random_range(-3, 3), v0.y + test_random_range(-3, 3), v0.z + test_random_range(-3, 3));
		rg_vec3 v2 = rg_vec3(v0.x + test_random_range(-3, 3), v0.y + test_random_range(-3, 3), v0.z + test_random_range(-3, 3));
		rg_vec3 target = rg_vec3((v0.x + v1.x + v2.x) / 3 + test_random_range(-2, 2), (v0.y + v1.y + v2.y) / 3 + test_random_range(-2, 2), (v0.z + v1.z + v2.z) / 3 + test_random_range(-2, 2));
		rg_vec2 bary;
		f32 t_bary = 0.0f;
		rg_vec3_set(&ray.origin, test_random_range(-8, 8), test_random_range(-8, 8), test_random_range(-8, 8));
		rg_vec3_sub(&target, &ray.origin, &ray.dir);
		rg_vec3_normalize(&ray.dir, &ray.dir);
		if (i & 1) rg_vec3_scale(&ray.dir, -1.0f, &ray.dir);
		t = 0.0f;
		int hit = rg_ray_triangle(&ray, &v0, &v1, &v2, &t);
		int hit_bary = rg_ray_triangle_bary(&ray, &v0, &v1, &v2, &bary, &t_bary);
		CHECK(hit == hit_bary);
		if (hit && hit_bary)
			CHECK_CLOSE(t, t_bary, 1e-5f * (1.0f + fabsf(t_bary)));
	}
}

static void test_aabb_transform(void)
{
	rg_aabb box, out;
	rg_mat4 m;
	rg_vec3 min = rg_vec3(1, 0, 0), max = rg_vec3(2, 1, 1);
	rg_vec3 axis = rg_vec3(0, 0, 1), offset = rg_vec3(3, -5, 7), one = rg_vec3(1, 1, 1);
	rg_quat q;
	rg_aabb_set(&box, &min, &max);

	rg_mat4_identity(&m);
	rg_aabb_transform(&box, &m, &out);
	check_vec3(&out.min, 1, 0, 0, 1e-6f);
	check_vec3(&out.max, 2, 1, 1, 1e-6f);
	rg_mat4_translate_make(&m, &offset);
	rg_aabb_transform(&box, &m, &out);
	check_vec3(&out.min, 4, -5, 7, 1e-6f);
	check_vec3(&out.max, 5, -4, 8, 1e-6f);
	// 90 degrees about z maps x to y.
	rg_quat_from_axis_angle(RG_HALF_PI, &axis, &q);
	rg_quat_mat4(&q, &m);
	rg_aabb_transform(&box, &m, &out);
	check_vec3(&out.min, -1, 1, 0, 1e-5f);
	check_vec3(&out.max, 0, 2, 1, 1e-5f);
	// Negative scale keeps min <= max.
	{
		rg_vec3 scale = rg_vec3(-2, 1, 1), zero = rg_vec3(0, 0, 0);
		rg_quat_identity(&q);
		rg_mat4_trs_make(&m, &zero, &q, &scale);
		rg_aabb_transform(&box, &m, &out);
		check_vec3(&out.min, -4, 0, 0, 1e-5f);
		check_vec3(&out.max, -2, 1, 1, 1e-5f);
	}
	// In-place transform.
	rg_mat4_translate_make(&m, &offset);
	out = box;
	rg_aabb_transform(&out, &m, &out);
	check_vec3(&out.min, 4, -5, 7, 1e-6f);
	check_vec3(&out.max, 5, -4, 8, 1e-6f);
	// Random affine transforms match the transformed-corner bounds.
	for (i32 i = 0; i < 500; ++i)
	{
		rg_vec3 random_axis = rg_vec3(test_random_range(-1, 1), test_random_range(-1, 1), test_random_range(-1, 1));
		rg_vec3 scale = rg_vec3(test_random_range(0.5f, 2), test_random_range(-2, -0.5f), test_random_range(0.5f, 2));
		rg_vec3 translation = rg_vec3(test_random_range(-10, 10), test_random_range(-10, 10), test_random_range(-10, 10));
		rg_aabb expected;
		f64 radius = 0.0;
		rg_vec3_normalize(&random_axis, &random_axis);
		rg_quat_from_axis_angle(test_random_range(-3, 3), &random_axis, &q);
		rg_mat4_trs_make(&m, &translation, &q, &scale);
		test_random_box(&box, 10.0f, 3.0f);
		rg_aabb_invalidate(&expected);
		for (i32 corner = 0; corner < 8; ++corner)
		{
			rg_vec3 p = rg_vec3(corner & 1 ? box.max.x : box.min.x, corner & 2 ? box.max.y : box.min.y, corner & 4 ? box.max.z : box.min.z);
			rg_vec3 transformed;
			rg_mat4_mulv3(&m, &p, 1.0f, &transformed);
			rg_aabb_expand_point(&expected, &transformed);
			radius = fmax(radius, fabs(transformed.x) + fabs(transformed.y) + fabs(transformed.z));
		}
		rg_aabb_transform(&box, &m, &out);
		check_vec3(&out.min, expected.min.x, expected.min.y, expected.min.z, (f32)(1e-5 * (1.0 + radius)));
		check_vec3(&out.max, expected.max.x, expected.max.y, expected.max.z, (f32)(1e-5 * (1.0 + radius)));
	}
	(void)one;
}

// f64 plane test over the eight corners; returns the smallest |distance| in margin.
static int reference_aabb_in_frustum(const rg_aabb* box, const rg_frustum* frustum, f64* margin)
{
	int inside = 1;
	*margin = DBL_MAX;
	for (i32 i = 0; i < RG_FRUSTUM_PLANE_COUNT; ++i)
	{
		const rg_plane* p = &frustum->planes[i];
		f64 best = -DBL_MAX;
		for (i32 corner = 0; corner < 8; ++corner)
		{
			f64 x = corner & 1 ? box->max.x : box->min.x;
			f64 y = corner & 2 ? box->max.y : box->min.y;
			f64 z = corner & 4 ? box->max.z : box->min.z;
			f64 dist = (f64)p->normal.x * x + (f64)p->normal.y * y + (f64)p->normal.z * z + (f64)p->d;
			if (dist > best) best = dist;
		}
		if (fabs(best) < *margin) *margin = fabs(best);
		if (best < 0.0) inside = 0;
	}
	return inside;
}

static int reference_sphere_in_frustum(const rg_sphere* sphere, const rg_frustum* frustum, f64* margin)
{
	int inside = 1;
	*margin = DBL_MAX;
	for (i32 i = 0; i < RG_FRUSTUM_PLANE_COUNT; ++i)
	{
		const rg_plane* p = &frustum->planes[i];
		f64 dist = (f64)p->normal.x * sphere->center.x + (f64)p->normal.y * sphere->center.y + (f64)p->normal.z * sphere->center.z + (f64)p->d + (f64)sphere->radius;
		if (fabs(dist) < *margin) *margin = fabs(dist);
		if (dist < 0.0) inside = 0;
	}
	return inside;
}

static void test_frustum_culling(void)
{
	enum { BOX_COUNT = 61 };
	static rg_aabb boxes[BOX_COUNT];
	static rg_sphere spheres[BOX_COUNT];
	static u8 flags[BOX_COUNT];
	rg_vec3 eye = rg_vec3(0, 0, 0), target = rg_vec3(0, 0, -1), up = rg_vec3(0, 1, 0);
	rg_mat4 view, projection, view_projection;
	rg_frustum frustum;
	rg_frustum_soa soa;
	rg_aabb box;
	rg_sphere sphere;
	rg_vec3 center, extents;

	// Symmetric 90 degree frustum looking down -z: |x|, |y| <= -z, 1 <= -z <= 100.
	rg_mat4_look_at_rh(&eye, &target, &up, &view);
	rg_mat4_perspective_rh_no(RG_HALF_PI, 1.0f, 1.0f, 100.0f, &projection);
	rg_mat4_mul(&projection, &view, &view_projection);
	rg_frustum_from_mat4_no(&view_projection, &frustum);
	for (i32 i = 0; i < RG_FRUSTUM_PLANE_COUNT; ++i)
		CHECK(frustum.planes[i].normal._pad == 0.0f);
	rg_frustum_soa_from_frustum(&frustum, &soa);
	for (i32 i = 0; i < RG_FRUSTUM_PLANE_COUNT; ++i)
	{
		CHECK(soa.nx[i] == frustum.planes[i].normal.x && soa.d[i] == frustum.planes[i].d);
		CHECK((soa.sel_x[i] != 0u) == (frustum.planes[i].normal.x >= 0.0f) && (soa.sel_z[i] != 0u) == (frustum.planes[i].normal.z >= 0.0f));
	}
	for (i32 i = RG_FRUSTUM_PLANE_COUNT; i < RG_FRUSTUM_SOA_LANES; ++i)
		CHECK(soa.nx[i] == 0.0f && soa.ny[i] == 0.0f && soa.nz[i] == 0.0f && soa.d[i] > 0.0f);

	// Inside, straddling each plane, fully outside each plane, degenerate, and enclosing boxes.
	const f32 centers[][3] = {{0, 0, -10}, {-10, 0, -10}, {10, 0, -10}, {0, -10, -10}, {0, 10, -10}, {0, 0, -1}, {0, 0, -100},
	                          {-13, 0, -10}, {13, 0, -10}, {0, -13, -10}, {0, 13, -10}, {0, 0, 0.5f}, {0, 0, -102}, {0, 0, -10}, {0, 0, 5}, {0, 0, -50}};
	const f32 half[][3] = {{1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1},
	                       {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {0, 0, 0}, {0, 0, 0}, {500, 500, 500}};
	const int expected[] = {1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1};
	for (size_t i = 0; i < RG_ARRAY_COUNT(expected); ++i)
	{
		rg_vec3_set(&center, centers[i][0], centers[i][1], centers[i][2]);
		rg_vec3_set(&extents, half[i][0], half[i][1], half[i][2]);
		rg_aabb_from_center_extents(&box, &center, &extents);
		CHECK(rg_aabb_in_frustum(&box, &frustum) == expected[i]);
		CHECK(rg_aabb_in_frustum_soa(&box, &soa) == expected[i]);
		CHECK(rg_frustum_cull_aabbs(&soa, &box, 1, flags) == (size_t)expected[i]);
		CHECK(flags[0] == (u8)expected[i]);
		// A sphere with the box's half size classifies the same way.
		rg_sphere_set(&sphere, &center, half[i][0]);
		CHECK(rg_sphere_in_frustum(&sphere, &frustum) == expected[i]);
		CHECK(rg_sphere_in_frustum_soa(&sphere, &soa) == expected[i]);
	}
	// Sphere straddling by less than its radius is visible; by more it is culled.
	rg_vec3_set(&center, 10.0f + 0.5f, 0, -10);
	rg_sphere_set(&sphere, &center, 1.0f);
	CHECK(rg_sphere_in_frustum(&sphere, &frustum));
	rg_vec3_set(&center, 10.0f + 1.5f, 0, -10);
	rg_sphere_set(&sphere, &center, 1.0f);
	CHECK(!rg_sphere_in_frustum(&sphere, &frustum));
	rg_sphere_set(&sphere, &center, 0.0f);
	CHECK(!rg_sphere_in_frustum_soa(&sphere, &soa));

	// Random boxes: f64 reference, SoA/batch agreement, odd counts, count 0.
	for (i32 iteration = 0; iteration < 40; ++iteration)
	{
		for (i32 k = 0; k < BOX_COUNT; ++k)
		{
			rg_vec3_set(&center, test_random_range(-40, 40), test_random_range(-40, 40), test_random_range(-110, 10));
			rg_vec3_set(&extents, test_random_range(0, 4), test_random_range(0, 4), test_random_range(0, 4));
			if ((k & 7) == 0) extents.data[k % 3] = 0.0f;
			if ((k & 15) == 1) rg_vec3_set(&extents, 60, 60, 60);
			rg_aabb_from_center_extents(&boxes[k], &center, &extents);
			rg_sphere_set(&spheres[k], &center, test_random_range(0, 6));
		}
		size_t count = (size_t)(iteration % 4 == 0 ? BOX_COUNT : (iteration % 4 == 1 ? 8 : (iteration % 4 == 2 ? 9 : 3)));
		size_t visible = rg_frustum_cull_aabbs(&soa, boxes, count, flags);
		size_t expected_visible = 0;
		for (size_t k = 0; k < count; ++k)
		{
			f64 margin;
			int single = rg_aabb_in_frustum(&boxes[k], &frustum);
			int reference = reference_aabb_in_frustum(&boxes[k], &frustum, &margin);
			CHECK(rg_aabb_in_frustum_soa(&boxes[k], &soa) == single);
			CHECK(flags[k] == (u8)single);
			if (margin > 1e-3) CHECK(single == reference);
			expected_visible += (size_t)single;
		}
		CHECK(visible == expected_visible);
		visible = rg_frustum_cull_spheres(&soa, spheres, count, flags);
		expected_visible = 0;
		for (size_t k = 0; k < count; ++k)
		{
			f64 margin;
			int single = rg_sphere_in_frustum(&spheres[k], &frustum);
			int reference = reference_sphere_in_frustum(&spheres[k], &frustum, &margin);
			CHECK(rg_sphere_in_frustum_soa(&spheres[k], &soa) == single);
			CHECK(flags[k] == (u8)single);
			if (margin > 1e-3) CHECK(single == reference);
			expected_visible += (size_t)single;
		}
		CHECK(visible == expected_visible);
	}
	flags[0] = 7;
	CHECK(rg_frustum_cull_aabbs(&soa, boxes, 0, flags) == 0);
	CHECK(rg_frustum_cull_spheres(&soa, spheres, 0, flags) == 0);
	CHECK(flags[0] == 7);
}

static void reference_slerp(const rg_quat* a, const rg_quat* b, f64 t, f64 out[4])
{
	f64 av[4] = {a->x, a->y, a->z, a->w}, bv[4] = {b->x, b->y, b->z, b->w};
	f64 cos_theta = av[0] * bv[0] + av[1] * bv[1] + av[2] * bv[2] + av[3] * bv[3];
	f64 angle, sin_theta, w0, w1, len = 0.0;
	if (cos_theta < 0.0)
	{
		cos_theta = -cos_theta;
		for (i32 k = 0; k < 4; ++k) bv[k] = -bv[k];
	}
	if (cos_theta > 1.0) cos_theta = 1.0;
	angle = acos(cos_theta);
	sin_theta = sin(angle);
	if (sin_theta < 1e-9) { w0 = 1.0 - t; w1 = t; }
	else { w0 = sin((1.0 - t) * angle) / sin_theta; w1 = sin(t * angle) / sin_theta; }
	for (i32 k = 0; k < 4; ++k) { out[k] = av[k] * w0 + bv[k] * w1; len += out[k] * out[k]; }
	len = sqrt(len);
	for (i32 k = 0; k < 4; ++k) out[k] /= len;
}

static void test_random_unit_quat(rg_quat* q)
{
	rg_vec3 axis = rg_vec3(test_random_range(-1, 1), test_random_range(-1, 1), test_random_range(-1, 1));
	f64 len;
	rg_vec3_normalize(&axis, &axis);
	rg_quat_from_axis_angle(test_random_range(-RG_PI, RG_PI), &axis, q);
	len = sqrt((f64)q->x * q->x + (f64)q->y * q->y + (f64)q->z * q->z + (f64)q->w * q->w);
	q->x = (f32)(q->x / len);
	q->y = (f32)(q->y / len);
	q->z = (f32)(q->z / len);
	q->w = (f32)(q->w / len);
}

static void test_quat_slerp(void)
{
	rg_vec3 axis = rg_vec3(0, 0, 1);
	rg_quat identity, quarter, eighth, out;
	rg_quat_identity(&identity);
	rg_quat_from_axis_angle(RG_HALF_PI, &axis, &quarter);
	rg_quat_from_axis_angle(RG_HALF_PI * 0.5f, &axis, &eighth);

	rg_quat_slerp(&identity, &quarter, 0.5f, &out);
	CHECK_CLOSE(out.x, eighth.x, 1e-5f);
	CHECK_CLOSE(out.y, eighth.y, 1e-5f);
	CHECK_CLOSE(out.z, eighth.z, 1e-5f);
	CHECK_CLOSE(out.w, eighth.w, 1e-5f);
	rg_quat_slerp(&identity, &quarter, 0.0f, &out);
	CHECK_CLOSE(rg_quat_dot(&out, &identity), 1.0f, 1e-5f);
	rg_quat_slerp(&identity, &quarter, 1.0f, &out);
	CHECK_CLOSE(rg_quat_dot(&out, &quarter), 1.0f, 1e-5f);
	// The shorter arc is taken when the inputs lie in opposite hemispheres.
	{
		rg_quat negated;
		rg_quat_set(&negated, -quarter.x, -quarter.y, -quarter.z, -quarter.w);
		rg_quat_slerp(&identity, &negated, 0.5f, &out);
		CHECK(rg_quat_dot(&out, &identity) > 0.0f);
		CHECK_CLOSE(fabsf(rg_quat_dot(&out, &eighth)), 1.0f, 1e-6f);
	}
	// Random pairs against an f64 reference, including nearly identical and
	// antipodal pairs, the interpolation endpoints, and extrapolation.
	for (i32 i = 0; i < 2000; ++i)
	{
		rg_quat a, b;
		f64 reference[4], len = 0.0;
		f32 t = test_random_unit();
		test_random_unit_quat(&a);
		test_random_unit_quat(&b);
		if (i % 10 == 0)
		{
			f64 blen;
			b = a;
			b.x += test_random_range(-0.01f, 0.01f);
			b.w += test_random_range(-0.01f, 0.01f);
			blen = sqrt((f64)b.x * b.x + (f64)b.y * b.y + (f64)b.z * b.z + (f64)b.w * b.w);
			b.x = (f32)(b.x / blen); b.y = (f32)(b.y / blen); b.z = (f32)(b.z / blen); b.w = (f32)(b.w / blen);
		}
		if (i % 10 == 1) rg_quat_set(&b, -b.x, -b.y, -b.z, -b.w);
		if (i % 20 == 2) t = 0.0f;
		if (i % 20 == 3) t = 1.0f;
		rg_quat_slerp(&a, &b, t, &out);
		reference_slerp(&a, &b, t, reference);
		for (i32 k = 0; k < 4; ++k)
		{
			CHECK_CLOSE(out.data[k], reference[k], 1e-5f);
			len += (f64)out.data[k] * out.data[k];
		}
		CHECK_CLOSE(sqrt(len), 1.0, 1e-5);
		t = test_random_range(-1.0f, 2.0f);
		rg_quat_slerp(&a, &b, t, &out);
		reference_slerp(&a, &b, t, reference);
		for (i32 k = 0; k < 4; ++k)
			CHECK_CLOSE(out.data[k], reference[k], 1e-4f);
	}
}

static void test_regressions(void)
{
	// Frustum culling must stay exact for boxes whose extent rounds away at
	// large coordinates: this box touches the x <= 1000000.0625 plane.
	{
		rg_frustum frustum;
		rg_frustum_soa soa;
		rg_aabb box;
		rg_aabb boxes[9];
		u8 flags[9];
		const f32 bound = 1000000.0625f;
		for (i32 i = 0; i < RG_FRUSTUM_PLANE_COUNT; ++i)
		{
			rg_vec3_set(&frustum.planes[i].normal, 0.0f, 0.0f, 0.0f);
			frustum.planes[i].d = 1e9f;
		}
		rg_vec3_set(&frustum.planes[0].normal, -1.0f, 0.0f, 0.0f);
		frustum.planes[0].d = bound;
		rg_vec3_set(&frustum.planes[1].normal, 0.0f, -1.0f, 0.0f);
		frustum.planes[1].d = bound;
		rg_vec3_set(&frustum.planes[2].normal, 1.0f, 0.0f, 0.0f);
		frustum.planes[2].d = 0.0f;
		rg_vec3_set(&box.min, bound, bound, -1.0f);
		rg_vec3_set(&box.max, bound + 0.0625f, bound + 0.0625f, 1.0f);
		CHECK(rg_aabb_in_frustum(&box, &frustum));
		rg_frustum_soa_from_frustum(&frustum, &soa);
		CHECK(rg_aabb_in_frustum_soa(&box, &soa));
		for (i32 i = 0; i < 9; ++i) boxes[i] = box;
		CHECK(rg_frustum_cull_aabbs(&soa, boxes, 9, flags) == 9u);
		// One ulp past the plane is outside on every path.
		box.min.x = bound + 0.0625f;
		box.max.x = bound + 0.125f;
		CHECK(!rg_aabb_in_frustum(&box, &frustum));
		CHECK(!rg_aabb_in_frustum_soa(&box, &soa));
		for (i32 i = 0; i < 9; ++i) boxes[i] = box;
		CHECK(rg_frustum_cull_aabbs(&soa, boxes, 9, flags) == 0u);
	}

	// Parallel slabs are unbounded: a ray from very far away still hits.
	{
		rg_aabb box;
		rg_ray ray;
		rg_ray_inv inv;
		f32 tmin = 0.0f, tmax = 0.0f;
		rg_vec3_set(&box.min, -1.0f, -1.0f, -1.0f);
		rg_vec3_set(&box.max, 1.0f, 1.0f, 1.0f);
		rg_vec3_set(&ray.origin, 0.0f, 0.0f, -1e31f);
		rg_vec3_set(&ray.dir, 0.0f, 0.0f, 1.0f);
		CHECK(rg_ray_aabb(&ray, &box, &tmin, &tmax));
		CHECK(tmin > 0.0f && tmax >= tmin);
		rg_ray_inv_make(&inv, &ray);
		CHECK(rg_ray_inv_aabb(&inv, &box, &tmin, &tmax));
	}

#if (defined(RG_MATH_SSE) || defined(RG_MATH_NEON)) && RG_MATH_MAX_PERF
	// Normalization of tiny vectors must not underflow with flush-to-zero on
	// (checked builds treat such vectors as zero-length by design).
	{
		rg_vec3 v, n;
		rg_vec4 v4, n4;
		f64 len;
#if defined(RG_MATH_SSE)
		const unsigned int csr = _mm_getcsr();
		_mm_setcsr(csr | 0x8000u); // FTZ
#else
		uint64_t fpcr;
		__asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
		__asm__ volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24))); // FZ
#endif
		rg_vec3_set(&v, 1.3278711e-19f, 0.0f, 0.0f);
		rg_vec3_normalize(&v, &n);
		len = sqrt((f64)n.x * n.x + (f64)n.y * n.y + (f64)n.z * n.z);
		CHECK(len > 0.999 && len < 1.001);
		rg_vec4_set(&v4, 0.0f, 2.0e-19f, 0.0f, 0.0f);
		rg_vec4_normalize(&v4, &n4);
		len = sqrt((f64)n4.x * n4.x + (f64)n4.y * n4.y + (f64)n4.z * n4.z + (f64)n4.w * n4.w);
		CHECK(len > 0.999 && len < 1.001);
		CHECK(rg_rsqrtf(2.0e-38f) > 0.999f * 7.0710678e18f && rg_rsqrtf(2.0e-38f) < 1.001f * 7.0710678e18f);
#if defined(RG_MATH_SSE)
		_mm_setcsr(csr);
#else
		__asm__ volatile("msr fpcr, %0" : : "r"(fpcr));
#endif
	}
#endif

	// rg_vec4_floor: negative non-integers, signed zero, and magnitudes beyond 2^31.
	{
		rg_vec4 input, output;
		rg_vec4_set(&input, -1.5f, -0.25f, -2.75f, -0.001f);
		rg_vec4_floor(&input, &output);
		CHECK(output.x == -2.0f && output.y == -1.0f && output.z == -3.0f && output.w == -1.0f);
		rg_vec4_set(&input, -0.0f, 0.0f, 0.999f, -0.999f);
		rg_vec4_floor(&input, &output);
		CHECK(output.x == 0.0f); // +0 or -0 both acceptable
		CHECK(output.y == 0.0f && output.z == 0.0f && output.w == -1.0f);
		rg_vec4_set(&input, 2147483648.0f, 3e9f, 1e10f, 1e30f);
		rg_vec4_floor(&input, &output);
		CHECK(output.x == 2147483648.0f && output.y == 3e9f && output.z == 1e10f && output.w == 1e30f);
		rg_vec4_set(&input, -2147483648.0f, -3e9f, -1e10f, -FLT_MAX);
		rg_vec4_floor(&input, &output);
		CHECK(output.x == -2147483648.0f && output.y == -3e9f && output.z == -1e10f && output.w == -FLT_MAX);
		rg_vec4_set(&input, 8388607.5f, -8388607.5f, 16777216.0f, -16777217.0f);
		rg_vec4_floor(&input, &output);
		CHECK(output.x == 8388607.0f && output.y == -8388608.0f && output.z == 16777216.0f && output.w == -16777216.0f);
	}
	// Normalization length error (vec3, vec4, quat) in the refined rsqrt mode. The
	// SSE estimate plus one Newton-Raphson step stays within 1e-6; the scalar
	// (Quake) estimate is only checked on the refined non-max-performance path.
#if RG_MATH_RSQRT_MODE == RG_MATH_RSQRT_MODE_RSQRT && (defined(RG_MATH_SIMD) || !RG_MATH_MAX_PERF)
#ifdef RG_MATH_SIMD
	const f64 length_tolerance = 1e-6;
#else
	const f64 length_tolerance = 1e-5;
#endif
	for (i32 i = 0; i < 16; ++i)
	{
		f32 scale = (i & 1) ? test_random_range(0.1f, 1.0f) : test_random_range(1.0f, 100.0f);
		rg_vec3 v3 = rg_vec3(test_random_range(-1, 1) * scale, test_random_range(-1, 1) * scale, test_random_range(-1, 1) * scale), n3;
		rg_vec4 v4 = rg_vec4(test_random_range(-1, 1) * scale, test_random_range(-1, 1) * scale, test_random_range(-1, 1) * scale, test_random_range(-1, 1) * scale), n4;
		rg_quat q = {{v4.x, v4.y, v4.z, v4.w}}, nq;
		rg_vec3_normalize(&v3, &n3);
		rg_vec4_normalize(&v4, &n4);
		rg_quat_normalize(&q, &nq);
		CHECK_CLOSE(sqrt((f64)n3.x * n3.x + (f64)n3.y * n3.y + (f64)n3.z * n3.z), 1.0, length_tolerance);
		CHECK_CLOSE(sqrt((f64)n4.x * n4.x + (f64)n4.y * n4.y + (f64)n4.z * n4.z + (f64)n4.w * n4.w), 1.0, length_tolerance);
		CHECK_CLOSE(sqrt((f64)nq.x * nq.x + (f64)nq.y * nq.y + (f64)nq.z * nq.z + (f64)nq.w * nq.w), 1.0, length_tolerance);
	}
#endif
	// rg_mat4_inv of a singular matrix: the assertion fires and no finite inverse results.
	{
		rg_mat4 singular, inverse;
		int asserts_before = math_assert_failures;
		int finite = 0;
		volatile f32 opaque_zero = 0.0f; // keeps the compiler from folding the division by zero
		rg_mat4_identity(&singular);
		singular.m[0] = opaque_zero; // zero first column
		rg_mat4_inv(&singular, &inverse);
		CHECK(math_assert_failures == asserts_before + 1);
		for (i32 i = 0; i < 16; ++i)
			finite += isfinite(inverse.m[i]) ? 1 : 0;
		CHECK(finite == 0);
		math_assert_failures = asserts_before;
	}
}

static void test_noise(void)
{
	rg_vec2 p2;
	rg_vec3 p3;
	rg_vec4 p4;
	u32 seed = 0x2545F491u;
	i32 i;

	// Integer lattice points are exactly zero in every dimension.
	rg_vec2_set(&p2, 2.0f, -3.0f);
	CHECK(rg_perlin_vec2(&p2) == 0.0f);
	rg_vec3_set(&p3, -7.0f, 11.0f, 250.0f);
	CHECK(rg_perlin_vec3(&p3) == 0.0f);
	rg_vec4_set(&p4, 1.0f, 0.0f, -1.0f, 1000.0f);
	CHECK(rg_perlin_vec4(&p4) == 0.0f);

	// Range, determinism, and continuity over pseudo-random points that
	// straddle cell boundaries (x just below an integer, then x + 1e-4).
	for (i = 0; i < 4096; ++i)
	{
		f32 a, b, x, y, z, w;
		seed ^= seed << 13;
		seed ^= seed >> 17;
		seed ^= seed << 5;
		x = (f32)(i32)(seed % 400u) - 200.0f - 5e-5f;
		y = (f32)(seed >> 8) * (1.0f / 16777216.0f) * 50.0f - 25.0f;
		z = (f32)(seed >> 12) * (1.0f / 1048576.0f) * 50.0f - 25.0f;
		w = (f32)(seed >> 16) * (1.0f / 65536.0f) * 50.0f - 25.0f;

		rg_vec2_set(&p2, x, y);
		a = rg_perlin_vec2(&p2);
		CHECK(a >= -1.0f && a <= 1.0f);
		CHECK(rg_perlin_vec2(&p2) == a);
		p2.x += 1e-4f;
		b = rg_perlin_vec2(&p2);
		CHECK(fabsf(b - a) <= 8.0f * 1e-4f);

		rg_vec3_set(&p3, x, y, z);
		a = rg_perlin_vec3(&p3);
		CHECK(a >= -1.0f && a <= 1.0f);
		CHECK(rg_perlin_vec3(&p3) == a);
		p3.x += 1e-4f;
		b = rg_perlin_vec3(&p3);
		CHECK(fabsf(b - a) <= 8.0f * 1e-4f);

		rg_vec4_set(&p4, x, y, z, w);
		a = rg_perlin_vec4(&p4);
		CHECK(a >= -1.0f && a <= 1.0f);
		CHECK(rg_perlin_vec4(&p4) == a);
		p4.x += 1e-4f;
		b = rg_perlin_vec4(&p4);
		CHECK(fabsf(b - a) <= 8.0f * 1e-4f);
	}

	// Regression anchors for the classic-Perlin output (change only if the
	// permutation/gradient tables or scales change).
	rg_vec2_set(&p2, 0.5f, 0.5f);
	CHECK_CLOSE(rg_perlin_vec2(&p2), -0.49995f, 1e-5f);
	rg_vec3_set(&p3, 1.3f, 2.7f, -0.4f);
	CHECK_CLOSE(rg_perlin_vec3(&p3), -0.106355f, 1e-5f);
	rg_vec4_set(&p4, 1.3f, 2.7f, -0.4f, 5.1f);
	CHECK_CLOSE(rg_perlin_vec4(&p4), 0.0065156f, 1e-5f);

	// Huge and non-finite coordinates stay finite and in range.
	{
		const f32 big[] = { 1e6f, -1e6f, 1e7f, -1e7f, 3e9f, -3e9f, 3e38f, -3e38f, INFINITY, -INFINITY };
		for (i = 0; i < (i32)(sizeof(big) / sizeof(big[0])); ++i)
		{
			f32 v;
			rg_vec2_set(&p2, big[i], 0.37f);
			v = rg_perlin_vec2(&p2);
			CHECK(v == v && v >= -1.0f && v <= 1.0f);
			rg_vec3_set(&p3, 0.37f, big[i], -big[i]);
			v = rg_perlin_vec3(&p3);
			CHECK(v == v && v >= -1.0f && v <= 1.0f);
			rg_vec4_set(&p4, big[i], 0.37f, -big[i], 0.71f);
			v = rg_perlin_vec4(&p4);
			CHECK(v == v && v >= -1.0f && v <= 1.0f);
		}
		rg_vec3_set(&p3, 1e6f + 0.37f, 0.5f, 0.5f);
		CHECK(rg_perlin_vec3(&p3) != 0.0f); // still non-degenerate below 2^24
	}
}

int main(void)
{
	test_layout();
	test_scalar();
	test_vectors();
	test_vec4_floor_and_sqrt();
	test_matrices();
	test_quaternions_and_euler();
	test_geometry();
	test_ray_sphere_roots();
	test_auxiliary_modules();
	test_noise();
	test_rotation_round_trips();
	test_camera_basis();
	test_projection_and_frustum();
	test_inverse_and_multiply_contracts();
	test_ray_aabb();
	test_ray_inv_aabb();
	test_ray_triangle();
	test_aabb_transform();
	test_frustum_culling();
	test_quat_slerp();
	test_regressions();
	CHECK(math_assert_failures == 0);

	printf("rg_math: %d checks, %d failures (%s)\n",
	       tests_run,
	       tests_failed,
#if defined(RG_MATH_SSE)
	       "SSE");
#elif defined(RG_MATH_NEON)
	       "NEON");
#else
	       "scalar");
#endif
	return tests_failed == 0 ? 0 : 1;
}
