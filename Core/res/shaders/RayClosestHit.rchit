#version 460
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "CommonUBO.glsl"

#define TRANSFORM_BUFFER_DESCRIPTOR_SET_IDX 1
#define TRANSFORM_BUFFER_DESCRIPTOR_BINDING 5
#include "Transforms.glsl"

layout(binding = 0, set = 1) uniform accelerationStructureEXT as;

struct RayPayload {
  vec3 colour;
  vec3 normal;
  float distance;
  bool first_hit;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

layout(set = 1, binding = 2, scalar) readonly buffer VertexPositionBuf { vec3 p[]; } vert_positions;

// Indices of triangle
layout(set = 1, binding = 3, scalar) readonly buffer IndexBuf { ivec3 i[]; } indices;

layout(set = 1, binding = 4, scalar) readonly buffer VertexNormalBuf { vec3 n[]; } vert_normals;

hitAttributeEXT vec3 attribs;

void main() {
  vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
  ivec3 i = indices.i[gl_PrimitiveID];

  vec3 p0 = vert_positions.p[i.x];
  vec3 p1 = vert_positions.p[i.y];
  vec3 p2 = vert_positions.p[i.z];

  vec3 n0 = vert_normals.n[i.x];
  vec3 n1 = vert_normals.n[i.y];
  vec3 n2 = vert_normals.n[i.z];

  vec3 p = p0 * bary.x + p1 * bary.y + p2 * bary.z;
  vec3 n = n0 * bary.x + n1 * bary.y + n2 * bary.z;

  mat4 transform = transforms.m[gl_InstanceCustomIndexEXT];
  vec3 pos = vec4(transform * vec4(p, 1.0)).xyz;
  n = normalize(transpose(inverse(mat3(transform))) * n);

  payload.colour += vec3(abs(sin(gl_InstanceCustomIndexEXT)), abs(cos(gl_InstanceCustomIndexEXT*53.4)), 1.0 - abs(sin(gl_InstanceCustomIndexEXT*26352.3))) * 0.25f;
  payload.normal = n;
  payload.distance = gl_RayTmaxEXT;

  if (payload.first_hit) {
    vec3 first_hit_col = vec3(abs(sin(gl_InstanceCustomIndexEXT)), abs(cos(gl_InstanceCustomIndexEXT*53.4)), 1.0 - abs(sin(gl_InstanceCustomIndexEXT*26352.3))) * 0.25f;

    payload.colour = vec3(abs(sin(gl_InstanceCustomIndexEXT)), abs(cos(gl_InstanceCustomIndexEXT*53.4)), 1.0 - abs(sin(gl_InstanceCustomIndexEXT*26352.3))) * 0.25f;
    payload.first_hit = false;
    vec3 ro = pos;
    vec3 rd = normalize(reflect(gl_WorldRayDirectionEXT, n));
    for (uint bounce = 0; bounce < 8; bounce++) {
      traceRayEXT(
        as,
        gl_RayFlagsOpaqueEXT,
        0xff,
        0,
        0,
        0,
        ro,
        0.001,
        rd, 
        10000,
        0
      );

      if (payload.distance < 0.f)
       break;

      ro += rd * payload.distance;
      rd = normalize(reflect(rd, payload.normal));
      
    }

    payload.colour *= first_hit_col;
  } 


}