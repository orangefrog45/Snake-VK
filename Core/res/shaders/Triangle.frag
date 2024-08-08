#version 450

layout(location = 0) out vec4 out_colour;
layout(set = 3, binding = 0) uniform sampler2D depth_tex;

layout(location = 0) in vec3 vs_col;
layout(location = 1) in vec2 vs_tex_coord;
layout(location = 2) in vec3 vs_normal;
layout(location = 3) in vec3 vs_world_pos;

#include "LightBuffers.glsli"
#include "TexMatBuffers.glsli"

const mat4 bias = mat4( 
  0.5, 0.0, 0.0, 0.0,
  0.0, 0.5, 0.0, 0.0,
  0.0, 0.0, 1.0, 0.0,
  0.5, 0.5, 0.0, 1.0 );

float CalcShadow() {
    vec4 frag_pos_light_space = ubo_light_data.light.light_transform * vec4(vs_world_pos, 1.0);

	vec3 proj_coords = (frag_pos_light_space / frag_pos_light_space.w).xyz;

	if (proj_coords.z > 1.0) {
		return 1; // early return, fragment out of range
	}

	float current_depth = proj_coords.z;
    proj_coords.y = -proj_coords.y;
    proj_coords.xy = proj_coords.xy * 0.5 + 0.5;
	float shadow = 0.0f;

	shadow += current_depth > texture(depth_tex, proj_coords.xy).r ? 1.0 : 0.0;
    return shadow;
}

void main() {
    vec3 light = vec3(1) * max(dot(normalize(vs_normal), -ubo_light_data.light.dir.xyz), 0);
    //out_colour = vec4(vec3(texture(depth_tex, vs_tex_coord).r), 1);
    out_colour = vec4(light * (1.0 - CalcShadow()), 1);
}