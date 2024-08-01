#version 450

layout(location = 0) out vec4 out_colour;
layout(binding = 1) uniform sampler2D s_tex;

layout(location = 0) in vec3 vs_col;
layout(location = 1) in vec2 vs_tex_coord;
layout(location = 2) in vec3 vs_normal;

void main() {
    out_colour = vec4(vs_normal, 1);
}