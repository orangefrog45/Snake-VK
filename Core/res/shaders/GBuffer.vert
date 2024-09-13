#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in vec3 in_tangent;

layout(location = 1) out vec2 vs_tex_coord;
layout(location = 2) out vec3 vs_normal;
layout(location = 3) out vec3 vs_world_pos;
layout(location = 4) out vec3 vs_tangent;
layout(location = 5) out mat3 vs_tbn;

#include "CommonUBO.glsl"

#define TRANSFORM_BUFFER_DESCRIPTOR_SET_IDX 2
#include "Transforms.glsl"

layout(push_constant) uniform pc {
    uint transform_idx;
    uint material_idx;
} push;

#define TRANSFORM transforms.m[push.transform_idx]

mat3 CalculateTbnMatrix(vec3 _t, vec3 _n) {
	vec3 t = normalize(vec3(mat3(TRANSFORM) * _t));
	vec3 n = normalize(vec3(mat3(TRANSFORM) * _n));

	t = normalize(t - dot(t, n) * n);
	vec3 b = cross(n, t);

	mat3 tbn = mat3(t, b, n);

	return tbn;
}

void main() {
    vec3 world_pos = vec4(TRANSFORM * vec4(in_position, 1.0)).xyz;

    vs_world_pos = world_pos;

    vs_tex_coord = in_tex_coord;
    vs_normal = transpose(inverse(mat3(TRANSFORM))) * in_normal;
    vs_tangent = in_tangent;
    vs_tbn = CalculateTbnMatrix(in_tangent, in_normal);
    gl_Position = common_ubo.proj * common_ubo.view * vec4(world_pos, 1.0);
}