#version 450

layout(location = 0) out vec4 out_col;

layout(location = 0) in vec4 vs_colour;

void main() {
    out_col = vs_colour;
}