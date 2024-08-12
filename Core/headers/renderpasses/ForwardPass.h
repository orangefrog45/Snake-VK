#pragma once
#include "core/Pipelines.h"
#include "core/Window.h"
#include "core/VkCommands.h"
#include "assets/MeshData.h"

namespace SNAKE {
	class ForwardPass {
	public:
		void Init(Window* p_window, Image2D* p_shadowmap_image);

		void RecordCommandBuffer(Image2D& output_image, class Scene& scene);

		void Render();

		vk::CommandBuffer GetCommandBuffer() {
			return *m_cmd_buffers[VulkanContext::GetCurrentFIF()].buf;
		}

	private:
		void CreateDepthResources(glm::vec2 window_dimensions);

		GraphicsPipeline m_graphics_pipeline;
		FullscreenImage2D m_depth_image;
		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> m_main_pass_descriptor_buffers;
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;
	};
}