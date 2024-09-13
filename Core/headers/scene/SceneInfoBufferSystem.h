#pragma once
#include "System.h"
#include "events/EventManager.h"
#include "core/DescriptorBuffer.h"

namespace SNAKE {
	class SceneInfoBufferSystem : public System {
	public:
		void OnSystemAdd() override;

		void UpdateUBO(FrameInFlightIndex frame_idx);

		void OnUpdate() override {

		}
		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> descriptor_buffers;
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> ubos;

	private:
		std::shared_ptr<DescriptorSetSpec> mp_descriptor_set_spec = nullptr;

		EventListener m_frame_start_listener;
	};
}