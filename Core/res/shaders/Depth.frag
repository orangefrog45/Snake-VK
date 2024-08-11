#version 450

#include "LightBuffers.glsl"

layout(location = 0) in vec3 in_normal;

void main() {
	float bias = clamp(0.0005 * tan(acos(clamp(dot(normalize(in_normal), ssbo_light_data.dir_light.dir.xyz), 0.0, 1.0))), 0.0, 0.01); // slope bias
	gl_FragDepth = gl_FragCoord.z + bias;
}