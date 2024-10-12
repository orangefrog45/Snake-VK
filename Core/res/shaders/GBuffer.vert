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
layout(location = 8) out vec4 vs_old_clip_pos;
layout(location = 9) out vec4 vs_current_clip_pos;

#include "CommonUBO.glsl"

#define TRANSFORM_BUFFER_DESCRIPTOR_SET_IDX 2
#include "Transforms.glsl"

layout(push_constant) uniform pc {
    uint transform_idx;
    uint material_idx;
    uint render_resolution_x;
    uint render_resolution_y;
    vec2 jitter_offset;
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
    vec3 current_frame_world_pos = vec4(TRANSFORM * vec4(in_position, 1.0)).xyz;
    vec3 prev_frame_world_pos = vec4(transforms_prev_frame.m[push.transform_idx] * vec4(in_position, 1.0)).xyz;

    vs_world_pos = current_frame_world_pos;

    vs_tex_coord = in_tex_coord;
    vs_normal = transpose(inverse(mat3(TRANSFORM))) * in_normal;
    vs_tangent = in_tangent;
    vs_tbn = CalculateTbnMatrix(in_tangent, in_normal);

    vec4 current_frame_clip_pos = common_ubo.proj * common_ubo.view * vec4(current_frame_world_pos, 1.0);
    vec4 prev_frame_clip_pos = common_ubo_prev_frame.proj * common_ubo_prev_frame.view * vec4(prev_frame_world_pos, 1.0);

    vs_old_clip_pos = prev_frame_clip_pos;
    vs_current_clip_pos = current_frame_clip_pos;

    vec4 jitter = vec4(push.jitter_offset*current_frame_clip_pos.w, 0, 0);

    jitter.x = ((jitter.x - 0.5) / push.render_resolution_x);
    jitter.y = ((jitter.y - 0.5) / push.render_resolution_y);

    gl_Position = current_frame_clip_pos + jitter;
}