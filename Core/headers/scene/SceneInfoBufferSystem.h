#pragma once
#include "System.h"
#include "events/EventManager.h"
#include "core/DescriptorBuffer.h"
#include "core/VkCommands.h"

namespace SNAKE {
	class SceneInfoBufferSystem : public System {
	public:
		void OnSystemAdd() override;

		void UpdateUBO(FrameInFlightIndex frame_idx);

		// Call this method whenever the last frames data will be unavailable or incorrect, e.g after a window resize
		void MarkPreviousFrameAsInvalid() {
			m_prev_frame_invalid = true;
		}

		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> descriptor_buffers;

		const S_VkBuffer& GetSceneInfoUBO(FrameInFlightIndex idx) {
			return m_ubos[idx];
		}

		const S_VkBuffer& GetOldSceneInfoUBO(FrameInFlightIndex idx) {
			return m_old_ubos[glm::min(idx - 1u, MAX_FRAMES_IN_FLIGHT - 1u)];
		}

	private:
		// Set to false automatically at the start of each frame
		bool m_prev_frame_invalid = true;

		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;

		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_ubos;

		// Ubos containing scene data from previous frames, used for motion vector generation
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_old_ubos;

		std::shared_ptr<DescriptorSetSpec> mp_descriptor_set_spec = nullptr;

		EventListener m_frame_start_listener;
		EventListener m_frame_end_listener;
	};
}