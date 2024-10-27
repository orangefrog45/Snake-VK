#pragma once
#include "components/Component.h"
#include "resources/S_VkBuffer.h"
#include "System.h"
#include "Entity.h"

namespace SNAKE {
	struct RaytracingInstanceBufferIdxComponent : public Component {
		RaytracingInstanceBufferIdxComponent(Entity* p_ent, uint32_t _idx) : Component(p_ent), idx(_idx) {}
		// Index of transform in global transform buffers available in shaders ("Transforms.glsl")
		uint32_t idx;
	};

	struct InstanceData {
		uint32_t transform_idx;

		// Index offset of mesh data in mesh buffers managed by MeshBufferManager
		uint32_t mesh_buffer_index_offset;

		// Vertex offset of mesh data in mesh buffers managed by MeshBufferManager
		uint32_t mesh_buffer_vertex_offset;

		// Instance flags - currently do nothing
		uint32_t flags;

		// Index of material managed by GlobalMaterialBufferManager
		uint32_t material_idx;
	};

	class RaytracingInstanceBufferSystem : public System {
	public:
		void OnSystemAdd() override;

		void UpdateBuffer(FrameInFlightIndex idx);

		const S_VkBuffer& GetStorageBuffer(FrameInFlightIndex idx);
	private:
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_storage_buffers;

		// first = entity to update, second = number of times updated (erased when equal to MAX_FRAMES_IN_FLIGHT)
		std::vector<std::pair<entt::entity, uint8_t>> m_instances_to_update;

		uint32_t m_current_buffer_idx = 0;

		EventListener m_mesh_event_listener;
		EventListener m_transform_event_listener;
		EventListener m_frame_start_event_listener;
	};

}