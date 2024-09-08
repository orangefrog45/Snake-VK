#version 460
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "CommonUBO.glsl"


#define RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_SET_IDX 1
#define RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_BINDING 7
#include "RaytracingInstances.glsl"


struct RayPayload {
  vec3 colour;
  vec3 normal;
  float distance;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

#define TEX_MAT_DESCRIPTOR_SET_IDX 2
#include "TexMatBuffers.glsl"

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
  vec3 n = n0 * bary.x + n1 * bary.y + n2 * bary.z;
  vec2 tc = tc0 * bary.x + tc1 * bary.y + tc2 * bary.z;

  mat4 transform = instance_data.transform;
  vec3 pos = vec4(transform * vec4(p, 1.0)).xyz;
  n = normalize(transpose(inverse(mat3(transform))) * n);

  Material material = material_ubo.materials[instance_data.material_idx];
  vec3 albedo = material.albedo.xyz;

  if (material.albedo_tex_idx != INVALID_GLOBAL_INDEX)
    albedo *= texture(textures[material.albedo_tex_idx], tc).rgb;

  payload.colour = albedo;
  payload.normal = n;
  payload.distance = gl_RayTmaxEXT;
}