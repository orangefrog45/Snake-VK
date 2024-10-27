#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

struct RayPayload {
  vec3 colour;
  vec3 normal;
  float distance;
  float roughness;
  uint mat_flags;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
  const vec3 gradientStart = vec3(0.5, 0.6, 1.0);
	const vec3 gradientEnd = vec3(1.0);
	vec3 unitDir = normalize(gl_WorldRayDirectionEXT);
	float t = 0.5 * (unitDir.y + 1.0);
  
 // payload.colour = ((1.0-t) * gradientStart + t * gradientEnd) * 0.25f;
  payload.colour = vec3(0);
    
  payload.distance = -1.f;
  
}