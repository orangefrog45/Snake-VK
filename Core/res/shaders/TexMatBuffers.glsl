#define MAT_FLAG_SAMPLED_ALBEDO uint(1 << 0)
#define MAT_FLAG_SAMPLED_NORMAL uint(1 << 1)
#define MAT_FLAG_SAMPLED_ROUGHNESS uint(1 << 2)
#define MAT_FLAG_SAMPLED_METALLIC uint(1 << 3)
#define MAT_FLAG_SAMPLED_AO uint(1 << 4)

struct Material {
    uint albedo_tex_idx;
    uint normal_tex_idx;
    uint roughness_tex_idx;
    uint metallic_tex_idx;
    uint ao_tex_idx;

    float roughness;
    float metallic;
    float ao;

    vec4 albedo;
    
    uint flags;
    uint pad0, pad1, pad2;
};

layout(set = 1, binding = 0) buffer MaterialUBO {
    Material materials[4096];
} material_ubo;

layout(set = 1, binding = 1) uniform sampler2D textures[4096];
