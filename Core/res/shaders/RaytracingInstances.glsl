// The below macros need to be defined
// Define RAYTRACING_EMISSIVE_BUFFER_DESCRIPTOR_BINDING as -1 if it's not needed
#if !defined(RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_SET_IDX) || !defined(RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_BINDING) || !defined(RAYTRACING_EMISSIVE_BUFFER_DESCRIPTOR_BINDING)
#error RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_SET_IDX or RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_BINDING or RAYTRACING_EMISSIVE_BUFFER_DESCRIPTOR_BINDING not defined
#endif

struct InstanceData {
    uint transform_idx;
    uint mesh_buffer_index_offset;
    uint mesh_buffer_vertex_offset;
    uint material_idx;
    uint num_mesh_indices;
};

layout(set = RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_SET_IDX, binding = RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_BINDING) readonly buffer InstanceDataBuf { InstanceData i[]; } rt_instances;

#if RAYTRACING_EMISSIVE_BUFFER_DESCRIPTOR_BINDING != -1
// Array of indices of all emissive objects in rt_instances buffer, used for quick emissive object lookup
layout(set = RAYTRACING_INSTANCE_BUFFER_DESCRIPTOR_SET_IDX, binding = RAYTRACING_EMISSIVE_BUFFER_DESCRIPTOR_BINDING) readonly buffer EmissiveIdxBuf { 
    uint num_emissive_instances;
    uint i[]; 
    } rt_emissive_instance_indices;
#endif