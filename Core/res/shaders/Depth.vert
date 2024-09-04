#version 450

#include "LightBuffers.glsl"

#define TRANSFORM_BUFFER_DESCRIPTOR_SET_IDX 0
#include "Transforms.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

layout(push_constant) uniform pc {
    uint transform_idx;
} push;


layout(location = 0) out vec3 out_normal;

void main() {
    out_normal = transpose(inverse(mat3(transforms.m[push.transform_idx]))) * in_normal;
    gl_Position = ssbo_light_data.dir_light.light_transform * transforms.m[push.transform_idx] * vec4(in_position, 1.0);
}