#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in vec3 in_tangent;

layout(location = 0) out vec3 vs_col;
layout(location = 1) out vec2 vs_tex_coord;
layout(location = 2) out vec3 vs_normal;
layout(location = 3) out vec3 vs_world_pos;
layout(location = 4) out vec3 vs_tangent;

#include "CommonUBO.glsl"

layout(push_constant) uniform pc {
    mat4 transform;
    uint material_idx;
} push;



void main() {
    vec3 world_pos = vec4(push.transform * vec4(in_position, 1.0)).xyz;

    vs_world_pos = world_pos;

    gl_Position = common_ubo.proj * common_ubo.view * vec4(world_pos, 1.0);
    vs_tex_coord = in_tex_coord;
    vs_normal = transpose(inverse(mat3(push.transform))) * in_normal;
    vs_tangent = in_tangent;
    vs_col = vec3(1);
}