#ifndef RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_SET_IDX
#error RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_SET_IDX not defined
#endif

struct InstanceData {
    uint transform_idx;
    
    uint mesh_buffer_index_offset;
    uint mesh_buffer_vertex_offset;
    uint flags;
    uint material_idx;
};

#ifdef RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_BINDING
layout(set = RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_SET_IDX, binding = RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_BINDING) readonly buffer InstanceDataBuf { InstanceData i[]; } rt_instances;
#else
layout(set = RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_SET_IDX, binding = 0) readonly buffer InstanceDataBuf { InstanceData i[]; } rt_instances;
#endif