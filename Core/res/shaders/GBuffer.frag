#version 450

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_rma;
layout(location = 3) out vec4 out_pixel_motion;

layout(set = 3, binding = 0) uniform sampler2D depth_tex;

layout(location = 1) in vec2 vs_tex_coord;
layout(location = 2) in vec3 vs_normal;
layout(location = 3) in vec3 vs_world_pos;
layout(location = 4) in vec3 vs_tangent;
layout(location = 5) in mat3 vs_tbn;
layout(location = 8) in vec4 vs_old_clip_pos;
layout(location = 9) in vec4 vs_current_clip_pos;

#include "CommonUBO.glsl"
#include "TexMatBuffers.glsl"

layout(push_constant) uniform pc {
    uint transform_idx;
    uint material_idx;
    uint render_resolution_x;
    uint render_resolution_y;
    vec2 jitter_offset;
} push;


vec2 CalcVelocity(vec4 current_clip_pos, vec4 old_clip_pos) {;
    return ((old_clip_pos.xy / old_clip_pos.w) - (current_clip_pos.xy / current_clip_pos.w)) * 0.5;
}

void main() {
    Material material = material_ubo.materials[push.material_idx];

    vec3 n;
    if (material.normal_tex_idx != INVALID_GLOBAL_INDEX) {
        n = normalize(vs_tbn * normalize(texture(textures[material.normal_tex_idx], vs_tex_coord).xyz * 2.0 - 1.0));
    } else {
        n = normalize(vs_normal);
    }

    vec3 albedo = material.albedo.rgb;
    if (material.albedo_tex_idx != INVALID_GLOBAL_INDEX)
        albedo *= texture(textures[material.albedo_tex_idx], vs_tex_coord).rgb;

    out_albedo = vec4(albedo, 1.0);
    out_pixel_motion = vec4(CalcVelocity(vs_current_clip_pos, vs_old_clip_pos), 0, 1);
    out_normal = vec4(n, 1.0);
    out_rma = vec4(material.roughness, material.metallic, material.ao, 1.0);
}