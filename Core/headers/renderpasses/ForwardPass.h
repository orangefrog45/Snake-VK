#pragma once
#include "core/Pipelines.h"
#include "core/Window.h"
#include "core/VkCommands.h"
#include "assets/MeshData.h"

namespace SNAKE {
	class ForwardPass {
	public:
		void Init(Window* p_window, Image2D* p_shadowmap_image);

		void RecordCommandBuffer(FrameInFlightIndex frame_idx, Window& window, uint32_t image_index, 
			class Scene& scene);

		void OnSwapchainInvalidate(glm::vec2 window_dimensions);

		vk::CommandBuffer GetCommandBuffer(FrameInFlightIndex idx) {
			return *m_cmd_buffers[idx].buf;
		}

	private:
		void CreateDepthResources(glm::vec2 window_dimensions);

		GraphicsPipeline m_graphics_pipeline;
		Image2D m_depth_image;
		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> m_main_pass_descriptor_buffers;
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;
	};
}