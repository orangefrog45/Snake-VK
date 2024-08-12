#pragma once
#include "System.h"
#include "core/DescriptorBuffer.h"


namespace SNAKE {
	class LightBufferSystem : public System {
	public:
		void OnSystemAdd() override;

		void UpdateLightSSBO();

		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> light_descriptor_buffers;
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> light_ssbos;
	private:
		EventListener m_frame_start_listener;

	};
}