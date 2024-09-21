#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

struct ShadowRayPayload {
  bool hit;
};

layout(location = 1) rayPayloadInEXT ShadowRayPayload shadow_payload;

void main() {
    shadow_payload.hit = false;
}