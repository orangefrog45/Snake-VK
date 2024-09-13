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

		void InitDescriptorBuffers(Image2D& output_image, Scene& scene, struct GBufferResources& output_gbuffer);

		void InitPipeline(std::weak_ptr<const DescriptorSetSpec> common_ubo_set);

		void RecordRenderCmdBuf(vk::CommandBuffer cmd, Image2D& output_image, DescriptorBuffer& common_ubo_db);

		void Init(Scene& scene, Image2D& output_image, std::weak_ptr<const DescriptorSetSpec> common_ubo_set, struct GBufferResources& gbuffer);
	private:
		std::unordered_map<struct MeshDataAsset*, BLAS> m_blas_map;

		EventListener frame_start_listener;

		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> rt_descriptor_buffers{};
		RtPipeline pipeline;
		std::shared_ptr<DescriptorSetSpec> rt_descriptor_set_spec = nullptr;

		std::array<TLAS, MAX_FRAMES_IN_FLIGHT> tlas_array;
	};
}