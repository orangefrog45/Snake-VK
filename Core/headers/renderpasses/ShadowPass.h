#pragma once
#include "core/VkCommon.h"
#include "core/VkCommands.h"
#include "images/Images.h"
#include "core/Pipelines.h"
#include "assets/MeshData.h"
#include "components/Component.h"
#include "rendering/RenderCommon.h"

namespace SNAKE {
	struct RenderableComponent : Component {
		RenderableComponent(Entity* p_entity) : Component(p_entity) {};
	};
	class ShadowPass {
	public:
		void Init();

		void RecordCommandBuffers(class Scene& scene, const SceneSnapshotData& data);

		vk::CommandBuffer GetCommandBuffer() { return *m_cmd_buffers[VulkanContext::GetCurrentFIF()].buf; }

		Image2D m_dir_light_shadow_map;
	private:
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;

		GraphicsPipeline m_pipeline;
	};
}