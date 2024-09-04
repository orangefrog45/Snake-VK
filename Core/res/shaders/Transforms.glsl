#ifndef TRANSFORM_BUFFER_DESCRIPTOR_SET_IDX
#error TRANSFORM_BUFFER_DESCRIPTOR_SET_IDX must be defined
#endif

#ifdef TRANSFORM_BUFFER_DESCRIPTOR_BINDING
layout(set = TRANSFORM_BUFFER_DESCRIPTOR_SET_IDX, binding = TRANSFORM_BUFFER_DESCRIPTOR_BINDING) readonly buffer TransformMatrices { mat4 m[]; } transforms;
#else
layout(set = TRANSFORM_BUFFER_DESCRIPTOR_SET_IDX, binding = 0) readonly buffer TransformMatrices { mat4 m[]; } transforms;
#endif