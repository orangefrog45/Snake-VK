#version 450

layout(location = 0) out vec4 out_colour;
layout(binding = 1) uniform sampler2D s_tex;

layout(location = 0) in vec3 vs_col;
layout(location = 1) in vec2 vs_tex_coord;
layout(location = 2) in vec3 vs_normal;

layout(set = 1, binding = 0) uniform sampler2D textures[4096];

struct Material {
    uint albedo_tex_idx;
    uint normal_tex_idx;
    uint roughness_tex_idx;
    uint metallic_tex_idx;
    uint ao_tex_idx;

    float roughness;
    float metallic;
    float ao;

    vec4 padding[2];
};

layout(set = 2, binding = 0) uniform MaterialUBO {
    Material materials[4096];
} material_ubo;

void main() {
    out_colour = vec4(texture(textures[material_ubo.materials[1].albedo_tex_idx], vs_tex_coord).rgb, 1);
}