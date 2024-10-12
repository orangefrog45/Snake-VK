#pragma once

#include "core/VkCommands.h"
#include "resources/Images.h"
#include "core/Pipelines.h"

namespace SNAKE {
	class TAA_ResolvePass {
	public:
		TAA_ResolvePass(Image2D* p_output_image, Image2D* p_velocity_image) : mp_output_image(p_output_image), mp_velocity_image(p_velocity_image) {};

		void Init();

		vk::CommandBuffer RecordCommandBuffer();
	private:
		Image2D* mp_output_image;
		Image2D* mp_velocity_image;

		Image2D m_history_image;
		Image2D m_prev_frame_velocity_image;

		ComputePipeline m_pipeline;
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;
		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> m_descriptor_buffers;
	};
}