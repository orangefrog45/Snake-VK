#pragma once
#include "resources/S_VkBuffer.h"
#include "scene/Scene.h"
#include "core/Pipelines.h"
#include "resources/TLAS.h"

namespace SNAKE {
	class Image2D;
	class TlasSystem;

	class RT {
	public:
		struct RtSettings {
			int32_t num_temporal_resamples = 1;
			int32_t num_spatial_resamples = 3;
		};

		void InitDescriptorBuffers(Image2D& output_image, Scene& scene, struct RaytracingResources& output_resources, TlasSystem& tlas_system);

		void InitPipeline(std::weak_ptr<const DescriptorSetSpec> common_ubo_set);

		void RecordRenderCmdBuf(vk::CommandBuffer cmd, Image2D& output_image, Scene& scene, RaytracingResources& output_resources, const RtSettings& settings);

		void Init(Scene& scene, Image2D& output_image, std::weak_ptr<const DescriptorSetSpec> common_ubo_set, RaytracingResources& resources);
	private:
		EventListener frame_start_listener;

		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> rt_descriptor_buffers{};
		RtPipeline pipeline;
		std::shared_ptr<DescriptorSetSpec> rt_descriptor_set_spec = nullptr;
	};
}