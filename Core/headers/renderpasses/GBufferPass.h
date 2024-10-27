#pragma once
#include "assets/AssetManager.h"
#include "core/DescriptorBuffer.h"
#include "core/Pipelines.h"
#include "core/VkCommands.h"
#include "rendering/RenderCommon.h"
#include "scene/Scene.h"


namespace SNAKE {

	class GBufferPass {
	public:
		struct GBufferPC {
			uint32_t transform_idx;
			uint32_t material_idx;
			glm::uvec2 render_resolution;
			glm::vec2 jitter_offset;
		};

	public:
		// output - resources which will be rendered to with RecordCommandBuffer
		void Init(Scene& scene, GBufferResources& output);

		// output_size - The size of the final colour output image produced in the pipeline (if DLSS is enabled this is different from the size of the gbuffer resources)
		void RecordCommandBuffer(GBufferResources& output, Scene& scene, glm::uvec2 output_size, vk::CommandBuffer buf);
	private:
		GraphicsPipeline m_mesh_pipeline;
		GraphicsPipeline m_particle_pipeline;

		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> m_descriptor_buffers;
	};
}