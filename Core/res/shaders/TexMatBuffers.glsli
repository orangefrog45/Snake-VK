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

layout(set = 1, binding = 0) uniform MaterialUBO {
    Material materials[4096];
} material_ubo;

layout(set = 1, binding = 0) uniform sampler2D textures[4096];
