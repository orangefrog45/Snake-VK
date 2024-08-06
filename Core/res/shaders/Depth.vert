#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

layout(push_constant) uniform pc {
    mat4 transform;
} push;

struct DirectionalLight {
    vec4 colour;
    vec4 dir;
    mat4 light_transform;
};

layout(set = 0, binding = 0) uniform LightData {
    DirectionalLight light;
} ubo_light_data;

layout(location = 0) out vec3 out_normal;

void main() {
    out_normal = in_normal;
    gl_Position = ubo_light_data.light.light_transform * push.transform * vec4(in_position, 1.0);
}