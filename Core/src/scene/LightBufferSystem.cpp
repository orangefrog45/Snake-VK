#include "scene/LightBufferSystem.h"


namespace SNAKE {
	void LightBufferSystem::OnSystemAdd() {
		constexpr size_t LIGHT_SSBO_SIZE = sizeof(LightSSBO);

		m_frame_start_listener.callback = [this](Event const* p_event) {
			auto* p_casted = dynamic_cast<FrameStartEvent const*>(p_event);
			UpdateLightSSBO(p_casted->frame_flight_index);
			};

		EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);

		for (FrameInFlightIndex i = 0; i < light_ssbos.size(); i++) {
			light_ssbos[i].CreateBuffer(LIGHT_SSBO_SIZE, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, 
				VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

			auto descriptor_spec = std::make_shared<DescriptorSetSpec>();
			descriptor_spec->AddDescriptor(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAllGraphics)
				.GenDescriptorLayout();

			light_descriptor_buffers[i].SetDescriptorSpec(descriptor_spec);
			light_descriptor_buffers[i].CreateBuffer(1);
			auto storage_buffer_info = light_ssbos[i].CreateDescriptorGetInfo();
			light_descriptor_buffers[i].LinkResource(storage_buffer_info.first, 0, 0);
		}
	}
}