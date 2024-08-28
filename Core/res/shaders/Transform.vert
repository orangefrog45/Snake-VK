#version 450
#define SNAKE_PERMUTATIONS(LINE)

#ifndef LINE
layout(location = 0) in vec3 in_position;
#endif

#include "CommonUBO.glsl"

layout(push_constant) uniform pc {
#ifdef LINE
    vec4[2] line_positions;
    vec4 colour;
#else
    mat4 transform;
#endif
} push;

#ifdef LINE
out layout(location = 0) vec4 vs_colour;
#endif

void main() {
#ifdef LINE
    gl_Position = common_ubo.proj * common_ubo.view * push.line_positions[gl_VertexIndex];
    vs_colour = push.colour;
#else
    gl_Position = common_ubo.proj * common_ubo.view * push.transform * vec4(in_position, 1.0);
#endif
}