#version 450

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec3 in_colour;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec3 vs_col;
layout(location = 1) out vec2 vs_tex_coord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;


void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(in_position, 0.0, 1.0);
    vs_tex_coord = in_tex_coord;
    vs_col = in_colour;
}