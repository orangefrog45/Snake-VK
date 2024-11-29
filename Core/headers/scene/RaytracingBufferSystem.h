#pragma once
#include "components/Component.h"
#include "resources/S_VkBuffer.h"
#include "System.h"
#include "Entity.h"

namespace SNAKE {
	// Contains index of instance data stored in raytracing instance buffer (in RaytracingInstances.glsl)
	struct RaytracingInstanceBufferIdxComponent : public Component {
		RaytracingInstanceBufferIdxComponent(Entity* p_ent, uint32_t _idx) : Component(p_ent), idx(_idx) {}
		uint32_t idx;
	};

	struct InstanceData {
		uint32_t transform_idx;

		// Index offset of mesh data in mesh buffers managed by MeshBufferManager
		uint32_t mesh_buffer_index_offset;

		// Vertex offset of mesh data in mesh buffers managed by MeshBufferManager
		uint32_t mesh_buffer_vertex_offset;

		// Index of material managed by GlobalMaterialBufferManager
		uint32_t material_idx;

		// Number of indices in this instances mesh
		uint32_t num_mesh_indices;
	};

	class RaytracingInstanceBufferSystem : public System {
	public:
		void OnSystemAdd() override;

		void UpdateInstanceBuffer();

		void UpdateEmissiveIdxBuffer();

		const S_VkBuffer& GetInstanceStorageBuffer(FrameInFlightIndex idx);

		const S_VkBuffer& GetEmissiveIdxStorageBuffer(FrameInFlightIndex idx);
	private:
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_storage_buffers_instances;
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_storage_buffers_emissive_indices;

		// first = entity to update, second = number of times updated (erased when equal to MAX_FRAMES_IN_FLIGHT)
		std::vector<std::pair<entt::entity, uint8_t>> m_instances_to_update;

		uint32_t m_current_buffer_idx = 0;

		EventListener m_mesh_event_listener;
		EventListener m_transform_event_listener;
		EventListener m_frame_start_event_listener;
	};

}