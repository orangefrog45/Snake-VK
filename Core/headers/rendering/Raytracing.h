#pragma once
#include "core/VkCommon.h"
#include "core/VkContext.h"
#include "resources/S_VkBuffer.h"
#include "assets/AssetManager.h"
#include "core/VkCommands.h"
#include "shaders/ShaderLibrary.h"
#include "assets/MeshData.h"
#include "components/TransformComponent.h"
#include "scene/Scene.h"
#include "resources/TLAS.h"

namespace SNAKE {
	class BLAS {
	public:
		void GenerateFromMeshData(MeshData& mesh_data, uint32_t submesh_index);

		vk::AccelerationStructureInstanceKHR GenerateInstance(TransformComponent& comp);
	private:
		uint32_t m_submesh_index = std::numeric_limits<uint32_t>::max();

		S_VkBuffer m_blas_buf;
		vk::UniqueAccelerationStructureKHR mp_blas;
		vk::DeviceAddress m_blas_handle = 0;
	};

	class RT {
	public:
		void InitTLAS(Scene& scene, FrameInFlightIndex idx);

		void UpdateTLAS();

		void InitDescriptorBuffers(Image2D& output_image, Scene& scene);

		void InitPipeline(const DescriptorSetSpec& common_ubo_set);

		// Records a raytraced gbuffer pass into cmd
		void RecordGBufferPassCmdBuf(vk::CommandBuffer cmd, class GBufferResources& output);

		void RecordRenderCmdBuf(vk::CommandBuffer cmd, Image2D& output_image, DescriptorBuffer& common_ubo_db);

		void Init(Scene& scene, Image2D& output_image, const DescriptorSetSpec& common_ubo_set_spec);

		void CreateShaderBindingTable();
	private:
		std::unordered_map<struct MeshDataAsset*, BLAS> m_blas_map;

		EventListener frame_start_listener;

		uint32_t sbt_group_count;
		uint32_t sbt_handle_size_aligned;
	
		std::shared_ptr<DescriptorSetSpec> rt_descriptor_set_spec = nullptr;

		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> rt_descriptor_buffers{};
		RtPipeline pipeline;

		vk::UniquePipelineLayout gbuffer_pipeline_layout;
		vk::UniquePipeline gbuffer_pipeline;

		S_VkBuffer sbt_ray_gen_buffer;
		S_VkBuffer sbt_ray_miss_buffer;
		S_VkBuffer sbt_ray_hit_buffer;

		std::array<TLAS, MAX_FRAMES_IN_FLIGHT> tlas_array;
	};
}