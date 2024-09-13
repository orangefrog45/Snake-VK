#pragma once
#include "core/Pipelines.h"
#include "core/Window.h"
#include "core/VkCommands.h"
#include "assets/MeshData.h"
#include "rendering/RenderCommon.h"

namespace SNAKE {
	class ForwardPass {
	public:
		void Init();

		void RecordCommandBuffer(Image2D& output_image, Image2D& depth_image, class Scene& scene, const struct SceneSnapshotData& data);

		vk::CommandBuffer GetCommandBuffer() {
			return *m_cmd_buffers[VkContext::GetCurrentFIF()].buf;
		}

	private:
		GraphicsPipeline m_graphics_pipeline;
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;
	};
}