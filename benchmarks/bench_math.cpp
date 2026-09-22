// Focused math throughput and regression benchmarks.
// Build bench_sink.c separately without LTCG; see build.bat bench_math.
// Optional argv[1] selects a substring of the case name.

#define RG_MATH_NO_IO
#include <rg_math.h>
#include "bench_common.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#ifdef RG_BENCH_CGLM
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4100) // cglm disables validity checks under /fp:fast.
#endif
extern "C" {
#include <cglm/cglm.h>
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif
#ifdef RG_BENCH_DIRECTXMATH
#include <DirectXMath.h>
#endif
#ifdef RG_BENCH_GLM
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#endif

enum { MATH_BATCH = 8192 };

struct MathInputs
{
	rg_vec3 a[MATH_BATCH], b[MATH_BATCH], direction[MATH_BATCH];
	rg_vec3 eye[MATH_BATCH], target[MATH_BATCH], up[MATH_BATCH];
	rg_vec4 v4a[MATH_BATCH], v4b[MATH_BATCH];
	rg_quat q0[MATH_BATCH], q1[MATH_BATCH];
	f32 angle[MATH_BATCH], scalar[MATH_BATCH];
	RG_ALIGN32 rg_mat4 matrices[4][MATH_BATCH];
	rg_mat4 rotation[MATH_BATCH], trs[MATH_BATCH];
	rg_mat3 rotation3[MATH_BATCH];
	rg_mat4 projection_no[MATH_BATCH], projection_zo[MATH_BATCH];
	rg_sphere spheres[MATH_BATCH];
	rg_ray rays_forward[MATH_BATCH], rays_inside[MATH_BATCH], rays_miss[MATH_BATCH];
};

static MathInputs inputs;

#ifdef RG_BENCH_CGLM
struct CglmInputs
{
	vec3 a[MATH_BATCH], b[MATH_BATCH];
	vec4 v4a[MATH_BATCH], v4b[MATH_BATCH];
	versor q0[MATH_BATCH], q1[MATH_BATCH];
	mat4 m0[MATH_BATCH], m1[MATH_BATCH];
};
static CglmInputs cglm_inputs;
#endif
#ifdef RG_BENCH_DIRECTXMATH
struct DirectXInputs
{
	DirectX::XMVECTOR a[MATH_BATCH], b[MATH_BATCH];
	DirectX::XMVECTOR v4a[MATH_BATCH], v4b[MATH_BATCH];
	DirectX::XMVECTOR q0[MATH_BATCH], q1[MATH_BATCH];
	DirectX::XMMATRIX m0[MATH_BATCH], m1[MATH_BATCH];
};
static DirectXInputs dx_inputs;
#endif
#ifdef RG_BENCH_GLM
struct GlmInputs
{
	glm::vec3 a[MATH_BATCH], b[MATH_BATCH];
	glm::vec4 v4a[MATH_BATCH], v4b[MATH_BATCH];
	glm::quat q0[MATH_BATCH], q1[MATH_BATCH];
	glm::mat4 m0[MATH_BATCH], m1[MATH_BATCH];
};
static GlmInputs glm_inputs;
#endif

static f32 random_unit(uint32_t* state)
{
	*state ^= *state << 13;
	*state ^= *state >> 17;
	*state ^= *state << 5;
	return (f32)(*state >> 8) * (1.0f / 16777216.0f);
}

static void prepare_inputs(void)
{
	uint32_t rng = UINT32_C(0x93E2417B);
	for (size_t i = 0; i < MATH_BATCH; ++i)
	{
		rg_vec3_set(&inputs.a[i], 0.5f + random_unit(&rng), -0.5f + random_unit(&rng), 0.5f + random_unit(&rng));
		rg_vec3_set(&inputs.b[i], random_unit(&rng), 0.5f + random_unit(&rng), random_unit(&rng));
		f32 inv_len = 1.0f / sqrtf(inputs.a[i].x * inputs.a[i].x + inputs.a[i].y * inputs.a[i].y + inputs.a[i].z * inputs.a[i].z);
		rg_vec3_set(&inputs.direction[i], inputs.a[i].x * inv_len, inputs.a[i].y * inv_len, inputs.a[i].z * inv_len);
		rg_vec3_set(&inputs.eye[i], 20.0f * random_unit(&rng), -5.0f + random_unit(&rng), 20.0f * random_unit(&rng));
		rg_vec3_set(&inputs.target[i], inputs.eye[i].x + inputs.direction[i].x, inputs.eye[i].y + inputs.direction[i].y, inputs.eye[i].z + inputs.direction[i].z);
		rg_vec3_set(&inputs.up[i], 0, 1, 0);
		rg_vec4_set(&inputs.v4a[i], inputs.a[i].x, inputs.a[i].y, inputs.a[i].z, 0.25f + random_unit(&rng));
		rg_vec4_set(&inputs.v4b[i], inputs.b[i].x, inputs.b[i].y, inputs.b[i].z, random_unit(&rng));
		inputs.angle[i] = -2.9f + 5.8f * random_unit(&rng);
		inputs.scalar[i] = 0.5f + random_unit(&rng);
		const f32 radius = 1.0f + inputs.scalar[i] * 0.25f;
		rg_sphere_set(&inputs.spheres[i], &inputs.eye[i], radius);
		rg_vec3 ray_direction = rg_vec3(0, 0, 1);
		rg_vec3 ray_origin = inputs.eye[i];
		ray_origin.z -= 4.0f * radius;
		rg_ray_set(&inputs.rays_forward[i], &ray_origin, &ray_direction);
		ray_origin.x += 3.0f * radius;
		rg_ray_set(&inputs.rays_miss[i], &ray_origin, &ray_direction);
		ray_origin = inputs.eye[i];
		ray_origin.x += 0.25f * radius;
		rg_ray_set(&inputs.rays_inside[i], &ray_origin, &ray_direction);
		rg_vec3 axis = inputs.direction[i];
		// Principal axes plus mixed rotations exercise all extraction branches.
		if ((i & 3u) != 3u)
		{
			rg_vec3_set(&axis, 0, 0, 0);
			axis.data[i & 3u] = 1.0f;
		}
		rg_quat_from_axis_angle(inputs.angle[i], &axis, &inputs.q0[i]);
		rg_quat_from_axis_angle(inputs.angle[i] * 0.7f, &inputs.direction[i], &inputs.q1[i]);
		rg_quat_mat4(&inputs.q0[i], &inputs.rotation[i]);
		rg_quat_mat3(&inputs.q0[i], &inputs.rotation3[i]);
		rg_vec3 scale = rg_vec3(1.0f, 1.0f, 1.0f);
		for (size_t j = 0; j < 4; ++j)
		{
			rg_mat4_trs_make(&inputs.matrices[j][i], &inputs.b[i], j & 1u ? &inputs.q1[i] : &inputs.q0[i], &scale);
		}
		rg_vec3_set(&scale, i & 1u ? 2.0f : -2.0f, 0.5f + random_unit(&rng), 1.5f);
		rg_mat4_trs_make(&inputs.trs[i], &inputs.eye[i], &inputs.q0[i], &scale);
		// Construct canonical LH projection inputs directly, so both versions
		// time identical decomposition/extraction inputs despite the old bug.
		rg_mat4_frustum_rh_no(-0.75f, 2.25f + inputs.scalar[i], -1.25f, 0.5f, 1.1f, 37.0f, &inputs.projection_no[i]);
		rg_mat4_frustum_rh_zo(-0.75f, 2.25f + inputs.scalar[i], -1.25f, 0.5f, 1.1f, 37.0f, &inputs.projection_zo[i]);
		rg_mat4* projections[] = {&inputs.projection_no[i], &inputs.projection_zo[i]};
		for (rg_mat4* p : projections)
		{
			p->m[8] = -p->m[8];
			p->m[9] = -p->m[9];
			p->m[10] = -p->m[10];
			p->m[11] = 1.0f;
		}
#ifdef RG_BENCH_CGLM
		memcpy(cglm_inputs.a[i], inputs.a[i].data, sizeof(vec3));
		memcpy(cglm_inputs.b[i], inputs.b[i].data, sizeof(vec3));
		memcpy(cglm_inputs.v4a[i], inputs.v4a[i].data, sizeof(vec4));
		memcpy(cglm_inputs.v4b[i], inputs.v4b[i].data, sizeof(vec4));
		memcpy(cglm_inputs.q0[i], inputs.q0[i].data, sizeof(versor));
		memcpy(cglm_inputs.q1[i], inputs.q1[i].data, sizeof(versor));
		memcpy(cglm_inputs.m0[i], inputs.matrices[0][i].m, sizeof(mat4));
		memcpy(cglm_inputs.m1[i], inputs.matrices[1][i].m, sizeof(mat4));
#endif
#ifdef RG_BENCH_DIRECTXMATH
		dx_inputs.a[i] = DirectX::XMVectorSet(inputs.a[i].x, inputs.a[i].y, inputs.a[i].z, 0);
		dx_inputs.b[i] = DirectX::XMVectorSet(inputs.b[i].x, inputs.b[i].y, inputs.b[i].z, 0);
		dx_inputs.v4a[i] = DirectX::XMVectorSet(inputs.v4a[i].x, inputs.v4a[i].y, inputs.v4a[i].z, inputs.v4a[i].w);
		dx_inputs.v4b[i] = DirectX::XMVectorSet(inputs.v4b[i].x, inputs.v4b[i].y, inputs.v4b[i].z, inputs.v4b[i].w);
		dx_inputs.q0[i] = DirectX::XMVectorSet(inputs.q0[i].x, inputs.q0[i].y, inputs.q0[i].z, inputs.q0[i].w);
		dx_inputs.q1[i] = DirectX::XMVectorSet(inputs.q1[i].x, inputs.q1[i].y, inputs.q1[i].z, inputs.q1[i].w);
		// DirectX uses row vectors: store the transpose of each rg matrix.
		DirectX::XMFLOAT4X4 m0, m1;
		memcpy(&m0, inputs.matrices[0][i].m, sizeof(m0));
		memcpy(&m1, inputs.matrices[1][i].m, sizeof(m1));
		dx_inputs.m0[i] = DirectX::XMLoadFloat4x4(&m0);
		dx_inputs.m1[i] = DirectX::XMLoadFloat4x4(&m1);
#endif
#ifdef RG_BENCH_GLM
		glm_inputs.a[i] = glm::vec3(inputs.a[i].x, inputs.a[i].y, inputs.a[i].z);
		glm_inputs.b[i] = glm::vec3(inputs.b[i].x, inputs.b[i].y, inputs.b[i].z);
		glm_inputs.v4a[i] = glm::vec4(inputs.v4a[i].x, inputs.v4a[i].y, inputs.v4a[i].z, inputs.v4a[i].w);
		glm_inputs.v4b[i] = glm::vec4(inputs.v4b[i].x, inputs.v4b[i].y, inputs.v4b[i].z, inputs.v4b[i].w);
		glm_inputs.q0[i] = glm::quat(inputs.q0[i].w, inputs.q0[i].x, inputs.q0[i].y, inputs.q0[i].z);
		glm_inputs.q1[i] = glm::quat(inputs.q1[i].w, inputs.q1[i].x, inputs.q1[i].y, inputs.q1[i].z);
		for (size_t col = 0; col < 4; ++col)
		{
			for (size_t row = 0; row < 4; ++row)
			{
				glm_inputs.m0[i][(glm::length_t)col][(glm::length_t)row] = inputs.matrices[0][i].m[col * 4 + row];
				glm_inputs.m1[i][(glm::length_t)col][(glm::length_t)row] = inputs.matrices[1][i].m[col * 4 + row];
			}
		}
#endif
	}
}

// Every input varies across the batch. Every output escapes to a separately
// compiled consumer after timing each batch; the compiler cannot retain only
// the final iteration or discard unused result components. No volatile writes
// or checksum arithmetic occur in the timed inner loop.
template<typename Result, typename Operation>
static double math_sample(void* context, uint64_t* checksum)
{
	Operation& operation = *(Operation*)context;
	static RG_ALIGN32 Result results[MATH_BATCH] = {};
	double elapsed = 0.0;
	uint64_t operations = 0;
	do
	{
		double start = rg_bench_now_ns();
		for (size_t i = 0; i < MATH_BATCH; ++i)
			operation(i, &results[i]);
		elapsed += rg_bench_now_ns() - start;
		*checksum = rg_bench_consume(results, sizeof(results));
		operations += MATH_BATCH;
	} while (elapsed < 20000000.0);
	return elapsed / (double)operations;
}

template<typename Result, typename Operation>
static void run_case(const char* name, Operation operation)
{
	rg_bench_run(name, &math_sample<Result, Operation>, &operation);
}

struct TrsResult { rg_vec3 translation; rg_quat rotation; rg_vec3 scale; };
struct ProjectionResult { f32 near_z, far_z, top, bottom, left, right; };
struct AxisResult { f32 first, second; };
struct RaySphereResult { f32 near_t, far_t; int hit; };

int main(int argc, char** argv)
{
	rg_bench_init(argc, argv);
	prepare_inputs();

	// The 15 operations in the documented 90-point engine profile.
	run_case<rg_vec3>("math.vec3_add", [](size_t i, rg_vec3* r) { rg_vec3_add(&inputs.a[i], &inputs.b[i], r); });
	run_case<rg_vec3>("math.vec3_scale", [](size_t i, rg_vec3* r) { rg_vec3_scale(&inputs.a[i], inputs.scalar[i], r); });
	run_case<f32>("math.vec3_dot", [](size_t i, f32* r) { *r = rg_vec3_dot(&inputs.a[i], &inputs.b[i]); });
	run_case<f32>("math.vec3_length", [](size_t i, f32* r) { *r = rg_vec3_len(&inputs.a[i]); });
	run_case<rg_vec3>("math.vec3_normalize", [](size_t i, rg_vec3* r) { rg_vec3_normalize(&inputs.a[i], r); });
	run_case<rg_vec3>("math.vec3_cross", [](size_t i, rg_vec3* r) { rg_vec3_cross(&inputs.a[i], &inputs.b[i], r); });
	run_case<rg_vec4>("math.vec4_add", [](size_t i, rg_vec4* r) { rg_vec4_add(&inputs.v4a[i], &inputs.v4b[i], r); });
	run_case<rg_vec4>("math.vec4_scale", [](size_t i, rg_vec4* r) { rg_vec4_scale(&inputs.v4a[i], inputs.scalar[i], r); });
	run_case<f32>("math.vec4_dot", [](size_t i, f32* r) { *r = rg_vec4_dot(&inputs.v4a[i], &inputs.v4b[i]); });
	run_case<f32>("math.vec4_length", [](size_t i, f32* r) { *r = rg_vec4_len(&inputs.v4a[i]); });
	run_case<rg_vec4>("math.vec4_normalize", [](size_t i, rg_vec4* r) { rg_vec4_normalize(&inputs.v4a[i], r); });
	run_case<rg_mat4>("math.mat4_mul", [](size_t i, rg_mat4* r) { rg_mat4_mul(&inputs.matrices[0][i], &inputs.matrices[1][i], r); });
	run_case<rg_vec4>("math.mat4_mulv4", [](size_t i, rg_vec4* r) { rg_mat4_mulv4(&inputs.matrices[0][i], &inputs.v4a[i], r); });
	run_case<rg_quat>("math.quat_mul", [](size_t i, rg_quat* r) { rg_quat_mul(&inputs.q0[i], &inputs.q1[i], r); });
	run_case<rg_quat>("math.quat_normalize", [](size_t i, rg_quat* r) { rg_quat_normalize(&inputs.q0[i], r); });

	// Component-wise operations retain the scalar helpers' configuration.
	run_case<rg_vec4>("math.vec4_floor", [](size_t i, rg_vec4* r) { rg_vec4_floor(&inputs.v4a[i], r); });
	run_case<rg_vec4>("math.vec4_sqrt", [](size_t i, rg_vec4* r) { rg_vec4_sqrt(&inputs.v4b[i], r); });
	run_case<RaySphereResult>("math.ray_sphere_forward", [](size_t i, RaySphereResult* r) {
		r->near_t = r->far_t = 0.0f;
		r->hit = rg_ray_sphere(&inputs.rays_forward[i], &inputs.spheres[i], &r->near_t, &r->far_t);
	});
	run_case<RaySphereResult>("math.ray_sphere_inside", [](size_t i, RaySphereResult* r) {
		r->near_t = r->far_t = 0.0f;
		r->hit = rg_ray_sphere(&inputs.rays_inside[i], &inputs.spheres[i], &r->near_t, &r->far_t);
	});
	run_case<RaySphereResult>("math.ray_sphere_miss", [](size_t i, RaySphereResult* r) {
		r->near_t = r->far_t = 0.0f;
		r->hit = rg_ray_sphere(&inputs.rays_miss[i], &inputs.spheres[i], &r->near_t, &r->far_t);
	});

	// Corrected camera, rotation-conversion, and projection paths.
	run_case<rg_mat4>("math.look_at_rh", [](size_t i, rg_mat4* r) { rg_mat4_look_at_rh(&inputs.eye[i], &inputs.target[i], &inputs.up[i], r); });
	run_case<rg_mat4>("math.look_at_lh", [](size_t i, rg_mat4* r) { rg_mat4_look_at_lh(&inputs.eye[i], &inputs.target[i], &inputs.up[i], r); });
	run_case<rg_mat4>("math.look_rh", [](size_t i, rg_mat4* r) { rg_mat4_look_rh(&inputs.eye[i], &inputs.direction[i], &inputs.up[i], r); });
	run_case<rg_mat4>("math.look_lh", [](size_t i, rg_mat4* r) { rg_mat4_look_lh(&inputs.eye[i], &inputs.direction[i], &inputs.up[i], r); });
	run_case<rg_mat4>("math.look_anyup_rh", [](size_t i, rg_mat4* r) { rg_mat4_look_anyup_rh(&inputs.eye[i], &inputs.direction[i], r); });
	run_case<rg_mat4>("math.look_anyup_lh", [](size_t i, rg_mat4* r) { rg_mat4_look_anyup_lh(&inputs.eye[i], &inputs.direction[i], r); });
	run_case<rg_quat>("math.quat_from_mat3", [](size_t i, rg_quat* r) { rg_quat_from_mat3(&inputs.rotation3[i], r); });
	run_case<rg_quat>("math.quat_from_mat4", [](size_t i, rg_quat* r) { rg_quat_from_mat4(&inputs.rotation[i], r); });
	run_case<TrsResult>("math.mat4_decompose", [](size_t i, TrsResult* r) { rg_mat4_decompose(&inputs.trs[i], &r->translation, &r->rotation, &r->scale); });
	run_case<TrsResult>("math.mat4_decompose_fast", [](size_t i, TrsResult* r) { rg_mat4_decompose_fast(&inputs.trs[i], &r->translation, &r->rotation, &r->scale); });
	run_case<rg_mat4>("math.frustum_lh_no", [](size_t i, rg_mat4* r) { rg_mat4_frustum_lh_no(-0.75f, 2.25f + inputs.scalar[i], -1.25f, 0.5f, 1.1f, 37.0f, r); });
	run_case<rg_mat4>("math.frustum_lh_zo", [](size_t i, rg_mat4* r) { rg_mat4_frustum_lh_zo(-0.75f, 2.25f + inputs.scalar[i], -1.25f, 0.5f, 1.1f, 37.0f, r); });
	run_case<ProjectionResult>("math.persp_decomp_lh_no", [](size_t i, ProjectionResult* r) { rg_mat4_persp_decomp_lh_no(&inputs.projection_no[i], &r->near_z, &r->far_z, &r->top, &r->bottom, &r->left, &r->right); });
	run_case<ProjectionResult>("math.persp_decomp_lh_zo", [](size_t i, ProjectionResult* r) { rg_mat4_persp_decomp_lh_zo(&inputs.projection_zo[i], &r->near_z, &r->far_z, &r->top, &r->bottom, &r->left, &r->right); });
	run_case<AxisResult>("math.persp_decomp_x_lh_no", [](size_t i, AxisResult* r) { rg_mat4_persp_decomp_x_lh_no(&inputs.projection_no[i], &r->first, &r->second); });
	run_case<AxisResult>("math.persp_decomp_x_lh_zo", [](size_t i, AxisResult* r) { rg_mat4_persp_decomp_x_lh_zo(&inputs.projection_zo[i], &r->first, &r->second); });
	run_case<AxisResult>("math.persp_decomp_y_lh_no", [](size_t i, AxisResult* r) { rg_mat4_persp_decomp_y_lh_no(&inputs.projection_no[i], &r->first, &r->second); });
	run_case<AxisResult>("math.persp_decomp_y_lh_zo", [](size_t i, AxisResult* r) { rg_mat4_persp_decomp_y_lh_zo(&inputs.projection_zo[i], &r->first, &r->second); });
	run_case<rg_frustum>("math.frustum_extract_no", [](size_t i, rg_frustum* r) { rg_frustum_from_mat4_no(&inputs.projection_no[i], r); });
	run_case<rg_frustum>("math.frustum_extract_zo", [](size_t i, rg_frustum* r) { rg_frustum_from_mat4_zo(&inputs.projection_zo[i], r); });
	run_case<rg_mat4>("math.mat4_inv_tr", [](size_t i, rg_mat4* r) { rg_mat4_inv_tr(&inputs.matrices[0][i], r); });
	run_case<rg_mat4>("math.mat4_inv_rigid", [](size_t i, rg_mat4* r) { rg_mat4_inv_tr(&inputs.matrices[0][i], r); });
	run_case<rg_mat4>("math.mat4_mul4", [](size_t i, rg_mat4* r) { rg_mat4_mul4(&inputs.matrices[0][i], &inputs.matrices[1][i], &inputs.matrices[2][i], &inputs.matrices[3][i], r); });
	run_case<rg_mat4>("math.mat4_mul4_aligned32", [](size_t i, rg_mat4* r) { rg_mat4_mul4_aligned32(&inputs.matrices[0][i], &inputs.matrices[1][i], &inputs.matrices[2][i], &inputs.matrices[3][i], r); });

#ifdef RG_BENCH_CGLM
	run_case<vec3>("cglm.vec3_add", [](size_t i, vec3* r) { glm_vec3_add(cglm_inputs.a[i], cglm_inputs.b[i], *r); });
	run_case<vec3>("cglm.vec3_scale", [](size_t i, vec3* r) { glm_vec3_scale(cglm_inputs.a[i], inputs.scalar[i], *r); });
	run_case<f32>("cglm.vec3_dot", [](size_t i, f32* r) { *r = glm_vec3_dot(cglm_inputs.a[i], cglm_inputs.b[i]); });
	run_case<f32>("cglm.vec3_length", [](size_t i, f32* r) { *r = glm_vec3_norm(cglm_inputs.a[i]); });
	run_case<vec3>("cglm.vec3_normalize", [](size_t i, vec3* r) { glm_vec3_normalize_to(cglm_inputs.a[i], *r); });
	run_case<vec3>("cglm.vec3_cross", [](size_t i, vec3* r) { glm_vec3_cross(cglm_inputs.a[i], cglm_inputs.b[i], *r); });
	run_case<vec4>("cglm.vec4_add", [](size_t i, vec4* r) { glm_vec4_add(cglm_inputs.v4a[i], cglm_inputs.v4b[i], *r); });
	run_case<vec4>("cglm.vec4_scale", [](size_t i, vec4* r) { glm_vec4_scale(cglm_inputs.v4a[i], inputs.scalar[i], *r); });
	run_case<f32>("cglm.vec4_dot", [](size_t i, f32* r) { *r = glm_vec4_dot(cglm_inputs.v4a[i], cglm_inputs.v4b[i]); });
	run_case<f32>("cglm.vec4_length", [](size_t i, f32* r) { *r = glm_vec4_norm(cglm_inputs.v4a[i]); });
	run_case<vec4>("cglm.vec4_normalize", [](size_t i, vec4* r) { glm_vec4_normalize_to(cglm_inputs.v4a[i], *r); });
	run_case<mat4>("cglm.mat4_mul", [](size_t i, mat4* r) { glm_mat4_mul(cglm_inputs.m0[i], cglm_inputs.m1[i], *r); });
	run_case<vec4>("cglm.mat4_mulv4", [](size_t i, vec4* r) { glm_mat4_mulv(cglm_inputs.m0[i], cglm_inputs.v4a[i], *r); });
	run_case<versor>("cglm.quat_mul", [](size_t i, versor* r) { glm_quat_mul(cglm_inputs.q0[i], cglm_inputs.q1[i], *r); });
	run_case<versor>("cglm.quat_normalize", [](size_t i, versor* r) { glm_quat_normalize_to(cglm_inputs.q0[i], *r); });
#endif
#ifdef RG_BENCH_DIRECTXMATH
	using namespace DirectX;
	run_case<XMVECTOR>("directx.vec3_add", [](size_t i, XMVECTOR* r) { *r = XMVectorAdd(dx_inputs.a[i], dx_inputs.b[i]); });
	run_case<XMVECTOR>("directx.vec3_scale", [](size_t i, XMVECTOR* r) { *r = XMVectorScale(dx_inputs.a[i], inputs.scalar[i]); });
	run_case<f32>("directx.vec3_dot", [](size_t i, f32* r) { *r = XMVectorGetX(XMVector3Dot(dx_inputs.a[i], dx_inputs.b[i])); });
	run_case<f32>("directx.vec3_length", [](size_t i, f32* r) { *r = XMVectorGetX(XMVector3Length(dx_inputs.a[i])); });
	run_case<XMVECTOR>("directx.vec3_normalize", [](size_t i, XMVECTOR* r) { *r = XMVector3Normalize(dx_inputs.a[i]); });
	run_case<XMVECTOR>("directx.vec3_cross", [](size_t i, XMVECTOR* r) { *r = XMVector3Cross(dx_inputs.a[i], dx_inputs.b[i]); });
	run_case<XMVECTOR>("directx.vec4_add", [](size_t i, XMVECTOR* r) { *r = XMVectorAdd(dx_inputs.v4a[i], dx_inputs.v4b[i]); });
	run_case<XMVECTOR>("directx.vec4_scale", [](size_t i, XMVECTOR* r) { *r = XMVectorScale(dx_inputs.v4a[i], inputs.scalar[i]); });
	run_case<f32>("directx.vec4_dot", [](size_t i, f32* r) { *r = XMVectorGetX(XMVector4Dot(dx_inputs.v4a[i], dx_inputs.v4b[i])); });
	run_case<f32>("directx.vec4_length", [](size_t i, f32* r) { *r = XMVectorGetX(XMVector4Length(dx_inputs.v4a[i])); });
	run_case<XMVECTOR>("directx.vec4_normalize", [](size_t i, XMVECTOR* r) { *r = XMVector4Normalize(dx_inputs.v4a[i]); });
	run_case<XMMATRIX>("directx.mat4_mul", [](size_t i, XMMATRIX* r) { *r = XMMatrixMultiply(dx_inputs.m1[i], dx_inputs.m0[i]); });
	run_case<XMVECTOR>("directx.mat4_mulv4", [](size_t i, XMVECTOR* r) { *r = XMVector4Transform(dx_inputs.v4a[i], dx_inputs.m0[i]); });
	// XMQuaternionMultiply(Q1, Q2) returns Q2*Q1.
	run_case<XMVECTOR>("directx.quat_mul", [](size_t i, XMVECTOR* r) { *r = XMQuaternionMultiply(dx_inputs.q1[i], dx_inputs.q0[i]); });
	run_case<XMVECTOR>("directx.quat_normalize", [](size_t i, XMVECTOR* r) { *r = XMQuaternionNormalize(dx_inputs.q0[i]); });
#endif
#ifdef RG_BENCH_GLM
	run_case<glm::vec3>("glm.vec3_add", [](size_t i, glm::vec3* r) { *r = glm_inputs.a[i] + glm_inputs.b[i]; });
	run_case<glm::vec3>("glm.vec3_scale", [](size_t i, glm::vec3* r) { *r = glm_inputs.a[i] * inputs.scalar[i]; });
	run_case<f32>("glm.vec3_dot", [](size_t i, f32* r) { *r = glm::dot(glm_inputs.a[i], glm_inputs.b[i]); });
	run_case<f32>("glm.vec3_length", [](size_t i, f32* r) { *r = glm::length(glm_inputs.a[i]); });
	run_case<glm::vec3>("glm.vec3_normalize", [](size_t i, glm::vec3* r) { *r = glm::normalize(glm_inputs.a[i]); });
	run_case<glm::vec3>("glm.vec3_cross", [](size_t i, glm::vec3* r) { *r = glm::cross(glm_inputs.a[i], glm_inputs.b[i]); });
	run_case<glm::vec4>("glm.vec4_add", [](size_t i, glm::vec4* r) { *r = glm_inputs.v4a[i] + glm_inputs.v4b[i]; });
	run_case<glm::vec4>("glm.vec4_scale", [](size_t i, glm::vec4* r) { *r = glm_inputs.v4a[i] * inputs.scalar[i]; });
	run_case<f32>("glm.vec4_dot", [](size_t i, f32* r) { *r = glm::dot(glm_inputs.v4a[i], glm_inputs.v4b[i]); });
	run_case<f32>("glm.vec4_length", [](size_t i, f32* r) { *r = glm::length(glm_inputs.v4a[i]); });
	run_case<glm::vec4>("glm.vec4_normalize", [](size_t i, glm::vec4* r) { *r = glm::normalize(glm_inputs.v4a[i]); });
	run_case<glm::mat4>("glm.mat4_mul", [](size_t i, glm::mat4* r) { *r = glm_inputs.m0[i] * glm_inputs.m1[i]; });
	run_case<glm::vec4>("glm.mat4_mulv4", [](size_t i, glm::vec4* r) { *r = glm_inputs.m0[i] * glm_inputs.v4a[i]; });
	run_case<glm::quat>("glm.quat_mul", [](size_t i, glm::quat* r) { *r = glm_inputs.q0[i] * glm_inputs.q1[i]; });
	run_case<glm::quat>("glm.quat_normalize", [](size_t i, glm::quat* r) { *r = glm::normalize(glm_inputs.q0[i]); });
#endif
	return 0;
}
