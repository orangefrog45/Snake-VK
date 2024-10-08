layout(set = 0, binding = 0) uniform CommonUBO {
    mat4 view;
    mat4 proj;
    vec4 cam_pos;
    vec4 cam_forward;
    uint frame_idx;
} common_ubo;

layout(set = 0, binding = 1) uniform CommonUBO_PrevFrame {
    mat4 view;
    mat4 proj;
    vec4 cam_pos;
    vec4 cam_forward;
    uint frame_idx;
} common_ubo_prev_frame;