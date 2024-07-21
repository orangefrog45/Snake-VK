#version 450

layout(location = 0) out vec4 out_colour;

layout(location = 0) in vec3 in_col;

void main() {
    out_colour = vec4(in_col, 1.0);
}