#version 450

struct DirectionalLight {
    vec4 colour;
    vec4 dir;
    mat4 light_transform;
};

layout(set = 0, binding = 0) uniform LightData {
    DirectionalLight light;
} ubo_light_data;

layout(location = 0) in vec3 in_normal;

void main() {
	float bias = clamp(0.0005 * tan(acos(clamp(dot(normalize(in_normal), ubo_light_data.light.dir.xyz), 0.0, 1.0))), 0.0, 0.01); // slope bias
	gl_FragDepth = gl_FragCoord.z + bias;
}