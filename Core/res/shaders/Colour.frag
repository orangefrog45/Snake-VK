#version 450

layout(location = 0) out vec4 out_col;

layout(location = 0) in vec4 in_colour;

void main() {
    out_col = in_colour;
}