#version 450

layout(location = 0) out vec4 out_colour;
layout(set = 3, binding = 0) uniform sampler2D depth_tex;

layout(location = 0) in vec3 vs_col;
layout(location = 1) in vec2 vs_tex_coord;
layout(location = 2) in vec3 vs_normal;
layout(location = 3) in vec3 vs_world_pos;
layout(location = 4) in vec3 vs_tangent;

#include "CommonUBO.glsl"
#include "LightBuffers.glsl"
#include "TexMatBuffers.glsl"

layout(push_constant) uniform pc {
    mat4 transform;
    uint material_idx;
} push;

mat3 CalculateTbnMatrix() {
	vec3 t = normalize(vec3(mat3(push.transform) * vs_tangent));
	vec3 n = normalize(vec3(mat3(push.transform) * vs_normal));

	t = normalize(t - dot(t, n) * n);
	vec3 b = cross(n, t);

	mat3 tbn = mat3(t, b, n);

	return tbn;
}


float CalcShadow() {
    vec4 frag_pos_light_space = ssbo_light_data.dir_light.light_transform * vec4(vs_world_pos, 1.0);

	vec3 proj_coords = (frag_pos_light_space / frag_pos_light_space.w).xyz;

	if (proj_coords.z > 1.0) {
		return 0.9; // early return, fragment out of range
	}

	float current_depth = proj_coords.z;
    proj_coords.y = -proj_coords.y;
    proj_coords.xy = proj_coords.xy * 0.5 + 0.5;
	float shadow = 0.0f;

	shadow += current_depth > texture(depth_tex, proj_coords.xy).r ? 1.0 : 0.0;
    return shadow;
}

void main() {
    Material material = material_ubo.materials[push.material_idx];
    vec3 n;

    if ((material.flags & MAT_FLAG_SAMPLED_NORMAL) != 0) {
        n = normalize(CalculateTbnMatrix() * (texture(textures[material.normal_tex_idx], vs_tex_coord).xyz * 2.0 - 1.0));
    } else {
        n = normalize(vs_normal);
    }

	vec3 v = normalize(common_ubo.cam_pos.xyz - vs_world_pos);
	vec3 r = reflect(-v, n);
	float n_dot_v = max(dot(n, v), 0.0);

    vec3 albedo = material.albedo.rgb;

    if ((material.flags & MAT_FLAG_SAMPLED_ALBEDO) != 0)
        albedo *= texture(textures[material.albedo_tex_idx], vs_tex_coord).rgb;

	vec3 f0 = vec3(0.04); // TODO: Support different values for more metallic objects
	f0 = mix(f0, albedo, material.metallic);

    vec3 light = CalcDirectionalLight(v, f0, n, material.roughness, material.metallic, albedo.rgb) * (1.0 - CalcShadow());
    //out_colour = vec4(vec3(texture(depth_tex, vs_tex_coord).r), 1);
    for (uint i = 0; i < ssbo_light_data.num_pointlights; i++) {
        light += CalcPointlight(ssbo_light_data.pointlights[i], v, f0, vs_world_pos, n, material.roughness, material.metallic, albedo.rgb);
    }

    for (uint i = 0; i < ssbo_light_data.num_pointlights; i++) {
        light += CalcSpotlight(ssbo_light_data.spotlights[i], v, f0, vs_world_pos, n, material.roughness, material.metallic, albedo.rgb);
    }

    out_colour = vec4(light, 1);
}