#version 450

#include "LightBuffers.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

layout(push_constant) uniform pc {
    mat4 transform;
} push;


layout(location = 0) out vec3 out_normal;

void main() {
    out_normal = transpose(inverse(mat3(push.transform))) * in_normal;
    gl_Position = ssbo_light_data.dir_light.light_transform * push.transform * vec4(in_position, 1.0);
}