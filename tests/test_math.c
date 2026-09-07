// rg_math public API correctness tests

#include "../src/rg_math.h"
#include "../src/rg_math.h"

#include <math.h>
#include <stdio.h>

static int tests_run;
static int tests_failed;

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
	CHECK(rg_imin(-4, 3) == -4);
	CHECK(rg_imax(-4, 3) == 3);
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
#if RG_MATH_MAX_PERF && !defined(RG_MATH_SSE)
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

int main(void)
{
	test_layout();
	test_scalar();
	test_vectors();
	test_matrices();
	test_quaternions_and_euler();
	test_geometry();
	test_auxiliary_modules();
	test_rotation_round_trips();
	test_camera_basis();
	test_projection_and_frustum();
	test_inverse_and_multiply_contracts();

	printf("rg_math: %d checks, %d failures (%s)\n",
	       tests_run,
	       tests_failed,
#ifdef RG_MATH_SSE
	       "SIMD");
#else
	       "scalar");
#endif
	return tests_failed == 0 ? 0 : 1;
}
