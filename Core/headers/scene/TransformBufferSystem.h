#pragma once
#include "components/Component.h"
#include "components/TransformComponent.h"
#include "resources/S_VkBuffer.h"
#include "System.h"
#include "Entity.h"
#include "Scene.h"
#include "core/VkCommon.h"
#include "core/VkCommands.h"


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

				if (p_casted->event_type == ComponentEventType::UPDATED) {
					m_transforms_to_update.emplace_back(p_casted->p_component->GetEntity()->GetComponent<TransformBufferIdxComponent>()->idx, 
						p_casted->p_component->GetMatrix());
				}
				else if (p_casted->event_type == ComponentEventType::ADDED) {
					p_casted->p_component->GetEntity()->AddComponent<TransformBufferIdxComponent>(m_current_buffer_idx++);
					m_transforms_to_update.emplace_back(m_current_buffer_idx - 1, p_casted->p_component->GetMatrix());
				}
				else if (p_casted->event_type == ComponentEventType::REMOVED) {
					// Delete all references to this instance from the update queue
					unsigned deletion_count = 0;
					std::vector<uint32_t> deletion_indices;
					auto* p_idx_comp = p_casted->p_component->GetEntity()->GetComponent<TransformBufferIdxComponent>();
					if (!p_idx_comp)
						return;

					auto buffer_idx = p_idx_comp->idx;
					for (uint32_t i = 0; i < m_transforms_to_update.size(); i++) {
						if (m_transforms_to_update[i].transform_buf_idx == buffer_idx)
							deletion_indices.push_back(i);
					}

					for (auto idx : deletion_indices) {
						m_transforms_to_update.erase(m_transforms_to_update.begin() + idx - deletion_count);
						deletion_count++;
					}
				}
			};

			EventManagerG::RegisterListener<ComponentEvent<TransformComponent>>(m_transform_event_listener);

			m_frame_start_event_listener.callback = [this]([[maybe_unused]] Event const* p_event) {
				uint8_t fif = VkContext::GetCurrentFIF();
				UpdateTransformBuffer(fif);
			};

			EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_event_listener);

			for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				m_transform_storage_buffers[i].CreateBuffer(sizeof(glm::mat4) * 4096, vk::BufferUsageFlagBits::eStorageBuffer | 
					vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferSrc,
					VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

				m_prev_frame_transform_storage_buffers[i].CreateBuffer(sizeof(glm::mat4) * 4096, vk::BufferUsageFlagBits::eStorageBuffer |
					vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
					VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

				m_transform_storage_buffers[i].Map();
				m_cmd_buffers[i].Init(vk::CommandBufferLevel::ePrimary);
			}
		};

		static constexpr uint8_t GetEraseBits() {
			uint8_t bits = 0u;
			for (FrameInFlightIndex i = 0u; i < MAX_FRAMES_IN_FLIGHT; i++) {
				bits = (bits | (1u << i));
				bits = (bits | (1u << (i + 4)));
			}
			return bits;
		}

		void UpdateTransformBuffer(FrameInFlightIndex fif) {
			uint32_t current_frame_idx = VkContext::GetCurrentFrameIdx();
			constexpr uint8_t ERASE_BITS = GetEraseBits();

			for (size_t i = 0; i < m_transforms_to_update.size(); i++) {
				auto& [update_bitset, transform_buf_idx, transform, frame_idx_transform_updated] = m_transforms_to_update[i];

				// If current buffer hasn't been updated with transform, update it
				if (!(update_bitset & (1u << fif))) {
					memcpy(reinterpret_cast<std::byte*>(m_transform_storage_buffers[fif].Map()) + transform_buf_idx * sizeof(glm::mat4),
						&transform, sizeof(glm::mat4));

					// Update flag that current buffer for this frame has been updated with transform
					update_bitset = update_bitset | (1u << fif);
				}

				// If the transform is at least one frame old, update the "old" buffer for this frame with it
				if ((!(update_bitset & (1u << (fif + 4)))) && (int)frame_idx_transform_updated < ((int)current_frame_idx - 1)) {
					memcpy(reinterpret_cast<std::byte*>(m_prev_frame_transform_storage_buffers[fif].Map()) + transform_buf_idx * sizeof(glm::mat4),
						&transform, sizeof(glm::mat4));

					// Update flag that "old" buffer for this frame has been updated with transform
					update_bitset = update_bitset | (1u << (fif + 4));
				}

				if (update_bitset == ERASE_BITS) {
					m_transforms_to_update.erase(m_transforms_to_update.begin() + i);
					i--;
				}
			}
		}

		const S_VkBuffer& GetTransformStorageBuffer(FrameInFlightIndex idx) {
			return m_transform_storage_buffers[idx];
		}

		// Returns the copied version of last frames transform storage buffer
		const S_VkBuffer& GetLastFramesTransformStorageBuffer(FrameInFlightIndex idx) {
			return m_prev_frame_transform_storage_buffers[idx];
		}

	private:
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;

		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_transform_storage_buffers;

		// Contains transform data from last rendered frame, used for motion vectors
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_prev_frame_transform_storage_buffers;

		struct TransformUpdateEntry {
			TransformUpdateEntry(uint32_t _transform_buf_idx, const glm::mat4& _transform) : 
				transform_buf_idx(_transform_buf_idx), transform(_transform), frame_idx_transform_updated(VkContext::GetCurrentFrameIdx()) {};

			// First 4 bits represent if the CURRENT transform buffer at FIF=(bit index) has been updated
			// Last 4 bits represent if the OLD transform buffer at FIF=(bit index - 4) has been updated
			// Entry is erased once all frames are updated
			uint8_t update_bitset = 0u;

			uint32_t transform_buf_idx;
			glm::mat4 transform;
			uint32_t frame_idx_transform_updated;
		};

		// first = entity to update, second = number of times updated (erased when equal to MAX_FRAMES_IN_FLIGHT)
		std::vector<TransformUpdateEntry> m_transforms_to_update;

		uint32_t m_current_buffer_idx = 0;

		EventListener m_transform_event_listener;
		EventListener m_frame_start_event_listener;
	};

}