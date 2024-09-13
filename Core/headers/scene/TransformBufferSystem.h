#pragma once
#include "components/Component.h"
#include "components/TransformComponent.h"
#include "resources/S_VkBuffer.h"
#include "System.h"
#include "Entity.h"
#include "Scene.h"


/* 
This system manages mesh transforms if the raster pipeline is active 
If the raytracing pipeline is active, RaytracingBufferSystem manages instance data instead
*/


namespace SNAKE {
	struct TransformBufferIdxComponent : public Component {
		TransformBufferIdxComponent(Entity* p_ent, uint32_t _idx) : Component(p_ent), idx(_idx) {}
		// Index of transform in global transform buffers available in shaders ("Transforms.glsl")
		uint32_t idx;
	};

	class TransformBufferSystem : public System {
	public:
		void OnSystemAdd() override {
			m_transform_event_listener.callback = [this](Event const* p_event) {
				auto* p_casted = dynamic_cast<ComponentEvent<TransformComponent> const*>(p_event);

				if (p_casted->event_type == ComponentEventType::UPDATED)
					m_transforms_to_update.push_back(std::make_pair(p_casted->p_component->GetEntity()->GetEnttHandle(), 0));
				else if (p_casted->event_type == ComponentEventType::ADDED) {
					p_casted->p_component->GetEntity()->AddComponent<TransformBufferIdxComponent>(m_current_buffer_idx++);
					m_transforms_to_update.push_back(std::make_pair(p_casted->p_component->GetEntity()->GetEnttHandle(), 0));
				}
			};

			EventManagerG::RegisterListener<ComponentEvent<TransformComponent>>(m_transform_event_listener);

			m_frame_start_event_listener.callback = [this]([[maybe_unused]] Event const* p_event) {
				UpdateTransformBuffer(VkContext::GetCurrentFIF());
			};

			EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_event_listener);

			for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				m_transform_storage_buffers[i].CreateBuffer(sizeof(glm::mat4) * 4096, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
					VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
				m_transform_storage_buffers[i].Map();
			}
		};

		void UpdateTransformBuffer(FrameInFlightIndex idx) {
			for (size_t i = 0; i < m_transforms_to_update.size(); i++) {
				auto& [ent, update_count] = m_transforms_to_update[i];
				update_count++;

				auto& reg = p_scene->GetRegistry();
				auto buf_idx = reg.get<TransformBufferIdxComponent>(ent).idx;

				memcpy(reinterpret_cast<std::byte*>(m_transform_storage_buffers[idx].Map()) + buf_idx * sizeof(glm::mat4), 
					&reg.get<TransformComponent>(ent).GetMatrix(), sizeof(glm::mat4));

				if (update_count == MAX_FRAMES_IN_FLIGHT) {
					m_transforms_to_update.erase(m_transforms_to_update.begin() + i);
					i--;
				}
			}
		}

		const S_VkBuffer& GetTransformStorageBuffer(FrameInFlightIndex idx) {
			return m_transform_storage_buffers[idx];
		}

	private:
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_transform_storage_buffers;

		// first = entity to update, second = number of times updated (erased when equal to MAX_FRAMES_IN_FLIGHT)
		std::vector<std::pair<entt::entity, uint8_t>> m_transforms_to_update;

		uint32_t m_current_buffer_idx = 0;

		EventListener m_transform_event_listener;
		EventListener m_frame_start_event_listener;
	};

}