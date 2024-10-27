#version 460
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "CommonUBO.glsl"


#define RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_SET_IDX 1
#define RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_BINDING 7
#include "RaytracingInstances.glsl"

#define TRANSFORM_BUFFER_DESCRIPTOR_SET_IDX 1
#define TRANSFORM_BUFFER_DESCRIPTOR_BINDING 14
#include "Transforms.glsl"


struct RayPayload {
  vec3 colour;
  vec3 normal;
  float distance;
  float roughness;
  uint mat_flags;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

struct ShadowRayPayload {
  bool hit;
};

layout(location = 1) rayPayloadEXT ShadowRayPayload shadow_payload;

#define TEX_MAT_DESCRIPTOR_SET_IDX 2
#include "TexMatBuffers.glsl"
#include "Util.glsl"

#define LIGHT_BUFFER_DESCRIPTOR_SET_BINDING_OVERRIDE
#define LIGHT_BUFFER_DESCRIPTOR_SET_IDX 1
#define LIGHT_BUFFER_DESCRIPTOR_BINDING 8
#include "LightBuffers.glsl"

layout(set = 1, binding = 0) uniform accelerationStructureEXT as;
layout(set = 1, binding = 2, scalar) readonly buffer VertexPositionBuf { vec3 p[]; } vert_positions;
layout(set = 1, binding = 3, scalar) readonly buffer IndexBuf { ivec3 i[]; } indices;
layout(set = 1, binding = 4, scalar) readonly buffer VertexNormalBuf { vec3 n[]; } vert_normals;
layout(set = 1, binding = 5, scalar) readonly buffer VertexTexCoordBuf { vec2 t[]; } vert_tex_coords;
layout(set = 1, binding = 6, scalar) readonly buffer VertexTangentBuf { vec3 t[]; } vert_tangents;

hitAttributeEXT vec3 attribs;

mat3 CalculateTbnMatrix(vec3 _t, vec3 _n, mat4 transform) {
	vec3 t = normalize(vec3(mat3(transform) * _t));
	vec3 n = normalize(vec3(mat3(transform) * _n));

	t = normalize(t - dot(t, n) * n);
	vec3 b = cross(n, t);

	mat3 tbn = mat3(t, b, n);

	return tbn;
}

float ShadowTest(vec3 ro, vec3 rd, float max_dist) {
  // Set initially to true, miss shader sets to false
  shadow_payload.hit = true;

  uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
  traceRayEXT(
    as,
    flags,
    0xff,
    0,
    0,
    1,
    ro,
    0.001,
    rd, 
    max_dist,
    1
  );

  return 1.0 - float(shadow_payload.hit);
}


void main() {
  vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

  InstanceData instance_data = rt_instances.i[gl_InstanceCustomIndexEXT];
  ivec3 i = indices.i[gl_PrimitiveID + instance_data.mesh_buffer_index_offset / 3] + ivec3(instance_data.mesh_buffer_vertex_offset);

  vec3 p0 = vert_positions.p[i.x];
  vec3 p1 = vert_positions.p[i.y];
  vec3 p2 = vert_positions.p[i.z];

  vec3 n0 = vert_normals.n[i.x];
  vec3 n1 = vert_normals.n[i.y];
  vec3 n2 = vert_normals.n[i.z];

  vec2 tc0 = vert_tex_coords.t[i.x];
  vec2 tc1 = vert_tex_coords.t[i.y];
  vec2 tc2 = vert_tex_coords.t[i.z];

  vec3 p = p0 * bary.x + p1 * bary.y + p2 * bary.z;
  vec3 n = normalize(n0 * bary.x + n1 * bary.y + n2 * bary.z);
  vec2 tc = tc0 * bary.x + tc1 * bary.y + tc2 * bary.z;

  mat4 transform = transforms.m[instance_data.transform_idx];
  vec3 pos = vec4(transform * vec4(p, 1.0)).xyz;
  n = normalize(transpose(inverse(mat3(transform))) * n);

  Material material = material_ubo.materials[instance_data.material_idx];
  vec3 albedo = material.albedo.xyz;
  if (material.albedo_tex_idx != INVALID_GLOBAL_INDEX)
    albedo *= texture(textures[material.albedo_tex_idx], tc).rgb;

  payload.mat_flags = material.flags;
  if (bool(material.flags & MAT_FLAG_EMISSIVE)) {
    payload.colour = albedo;
    payload.distance = gl_RayTmaxEXT;
    return;
  }



  vec3 v = normalize(-gl_WorldRayDirectionEXT);
	vec3 r = reflect(-v, n);
	float n_dot_v = max(dot(n, v), 0.0);

  vec3 f0 = vec3(0.04); // TODO: Support different values for more metallic objects
  f0 = mix(f0, albedo, material.metallic);

  vec3 biased_pos = pos + n * 0.01;

  vec3 light = CalcDirectionalLight(v, f0, n, material.roughness, material.metallic, albedo.rgb) * ShadowTest(biased_pos, ssbo_light_data.dir_light.dir.xyz, 10000);

  if (ssbo_light_data.num_pointlights > 0) {
    light += CalcPointlight(ssbo_light_data.pointlights[0], v, f0, pos, n, material.roughness, material.metallic, albedo.rgb) * ShadowTest(biased_pos, ssbo_light_data.pointlights[0].pos.xyz - biased_pos, 10000);;
  }
  payload.colour = light;
  payload.normal = n;
  payload.distance = gl_RayTmaxEXT;
  payload.roughness = material.roughness;
}