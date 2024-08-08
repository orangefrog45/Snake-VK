#version 450

#include "LightBuffers.glsli"

layout(location = 0) in vec3 in_normal;

void main() {
	float bias = clamp(0.0005 * tan(acos(clamp(dot(normalize(in_normal), ubo_light_data.light.dir.xyz), 0.0, 1.0))), 0.0, 0.01); // slope bias
	gl_FragDepth = gl_FragCoord.z + bias;
}