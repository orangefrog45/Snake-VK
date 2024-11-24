layout(set = 0, binding = 0) uniform CommonUBO {
    mat4 view;
    mat4 proj;
    mat4 proj_view;
    vec4 cam_pos;
    vec4 cam_forward;
    uint frame_idx;
    float time_elapsed;
    float delta_time;
} common_ubo;

layout(set = 0, binding = 1) uniform CommonUBO_PrevFrame {
    mat4 view;
    mat4 proj;
    mat4 proj_view;
    vec4 cam_pos;
    vec4 cam_forward;
    uint frame_idx;
    float time_elapsed;
    float delta_time;
} common_ubo_prev_frame;