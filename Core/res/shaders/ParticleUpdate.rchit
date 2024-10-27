#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

struct RayPayload {
    vec3 contact_normal;
    vec3 contact_velocity;
    vec3 contact_position;
    float distance;
};


layout(location = 0) rayPayloadInEXT RayPayload payload;

layout(set = 1, binding = 2, scalar) readonly buffer IndexBuf { ivec3 i[]; } indices;
layout(set = 1, binding = 3, scalar) readonly buffer VertexNormalBuf { vec3 n[]; } vert_normals;
layout(set = 1, binding = 7, scalar) readonly buffer VertexPosBuf { vec3 p[]; } vert_positions;
#define RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_SET_IDX 1
#define RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_BINDING 4
#include "RaytracingInstances.glsl"

#define TRANSFORM_BUFFER_DESCRIPTOR_SET_IDX 1
#define TRANSFORM_BUFFER_DESCRIPTOR_BINDING 5
#include "Transforms.glsl"

hitAttributeEXT vec3 attribs;

void main() {
  vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

  InstanceData instance_data = rt_instances.i[gl_InstanceCustomIndexEXT];
  ivec3 i = indices.i[gl_PrimitiveID + instance_data.mesh_buffer_index_offset / 3] + ivec3(instance_data.mesh_buffer_vertex_offset);
  
  vec3 n0 = vert_normals.n[i.x];
  vec3 n1 = vert_normals.n[i.y];
  vec3 n2 = vert_normals.n[i.z];

  vec3 p0 = vert_positions.p[i.x];
  vec3 p1 = vert_positions.p[i.y];
  vec3 p2 = vert_positions.p[i.z];
  
  vec3 n = n0 * bary.x + n1 * bary.y + n2 * bary.z;
  vec3 p = p0 * bary.x + p1 * bary.y + p2 * bary.z;

  mat4 transform = transforms.m[instance_data.transform_idx];
  mat4 transform_prev_frame = transforms_prev_frame.m[instance_data.transform_idx];
  n = normalize(transpose(inverse(mat3(transform))) * n);

  vec3 contact_position = (transform * vec4(p, 1)).xyz;

  payload.distance = gl_RayTmaxEXT;
  payload.contact_normal = n;
  const float dt = 1.0 / 20.0;
  payload.contact_position = contact_position;
  payload.contact_velocity = vec3(contact_position - (transform_prev_frame * vec4(p, 1)).xyz) / dt;
}