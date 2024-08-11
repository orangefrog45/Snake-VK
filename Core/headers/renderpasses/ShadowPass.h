#pragma once
#include "core/VkCommon.h"
#include "core/VkCommands.h"
#include "textures/Textures.h"
#include "core/Pipelines.h"
#include "assets/MeshData.h"
#include "components/Component.h"

namespace SNAKE {
	struct RenderableComponent : Component {
		RenderableComponent(Entity* p_entity) : Component(p_entity) {};
	};
	class ShadowPass {
	public:
		void Init();

		void RecordCommandBuffers(FrameInFlightIndex frame_idx, class Scene* p_scene);

		vk::CommandBuffer GetCommandBuffer(FrameInFlightIndex idx) { return *m_cmd_buffers[idx].buf; }

		Image2D m_dir_light_shadow_map;
	private:
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;

		GraphicsPipeline m_pipeline;

	};
}