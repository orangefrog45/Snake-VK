layout(set = 0, binding = 0) uniform CommonUBO {
    mat4 view;
    mat4 proj;
    vec4 cam_pos;
    vec4 cam_forward;
} common_ubo;