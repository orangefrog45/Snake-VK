#pragma once
#include "assets/MeshData.h"
#include "components/Component.h"
#include "core/Pipelines.h"
#include "core/VkCommands.h"
#include "core/VkCommon.h"
#include "rendering/RenderCommon.h"
#include "resources/Images.h"

namespace SNAKE {
	struct RenderableComponent : Component {
		RenderableComponent(Entity* p_entity) : Component(p_entity) {};
	};
	class ShadowPass {
	public:
		void Init();

		void RecordCommandBuffers(class Scene& scene, const struct SceneSnapshotData& data);

		vk::CommandBuffer GetCommandBuffer() { return *m_cmd_buffers[VkContext::GetCurrentFIF()].buf; }

		Image2D m_dir_light_shadow_map;
	private:
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;

		GraphicsPipeline m_pipeline;
	};
}