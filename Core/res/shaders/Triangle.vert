#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec3 vs_col;
layout(location = 1) out vec2 vs_tex_coord;
layout(location = 2) out vec3 vs_normal;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform pc {
    mat4 transform;
} push;


void main() {
    gl_Position = ubo.proj * ubo.view * push.transform * vec4(in_position, 1.0);
    vs_tex_coord = in_tex_coord;
    vs_normal = in_normal;
    vs_col = vec3(1);
}