#include "pch/pch.h"
#include "assets/AssetManager.h"
#include "core/VkCommon.h"
#include "scene/ParticleSystem.h"
#include "scene/SceneInfoBufferSystem.h"
#include "scene/Scene.h"
#include "scene/TlasSystem.h"
#include "scene/RaytracingBufferSystem.h"
#include "scene/TransformBufferSystem.h"

// For integer log
#include <glm/gtc/integer.hpp>

using namespace SNAKE;

void ParticleSystem::OnSystemAdd() {
	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_cmd_buffers[i].Init(vk::CommandBufferLevel::ePrimary);
	}

	InitPipelines();
	InitParticles();
	
	m_frame_start_listener.callback = [&]([[maybe_unused]] Event const*) {
		auto fif = VkContext::GetCurrentFIF();
		unsigned PARTICLE_DATA_SIZE = sizeof(glm::vec4) * 2 * m_num_particles;

		auto cmd = *m_cmd_buffers[fif].buf;
		cmd.reset();

		vk::CommandBufferBeginInfo begin_info{};
		SNK_CHECK_VK_RESULT(cmd.begin(begin_info));

		FrameInFlightIndex old_idx = (FrameInFlightIndex)glm::min(fif - 1u, MAX_FRAMES_IN_FLIGHT - 1u);
		CopyBuffer(m_ptcl_buffers[old_idx].buffer, m_ptcl_buffers_prev_frame[fif].buffer, PARTICLE_DATA_SIZE, 0u, 0u, *m_cmd_buffers[fif].buf);
		CopyBuffer(m_ptcl_buffers[old_idx].buffer, m_ptcl_buffers[fif].buffer, PARTICLE_DATA_SIZE, 0u, 0u, *m_cmd_buffers[fif].buf);

		memcpy(m_params_buffers[fif].Map(), &parameters, sizeof(PhysicsParamsUBO));

		UpdateParticles();
	};

	EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);
}

void ParticleSystem::OnSystemRemove() {
	EventManagerG::DeregisterListener(m_frame_start_listener);
}

void ParticleSystem::InitPipelines() {
	m_compute_pipelines[ParticleComputeShader::INIT].Init("res/shaders/Particlecomp_10000000.spv");
	m_compute_pipelines[ParticleComputeShader::CELL_KEY_GENERATION].Init("res/shaders/Particlecomp_01000000.spv");
	m_compute_pipelines[ParticleComputeShader::BITONIC_CELL_KEY_SORT].Init("res/shaders/Particlecomp_00100000.spv");
	m_compute_pipelines[ParticleComputeShader::CELL_KEY_START_IDX_GENERATION].Init("res/shaders/Particlecomp_00010000.spv");

	RtPipelineBuilder rt_builder{};
	rt_builder.AddShader(vk::ShaderStageFlagBits::eRaygenKHR, "res/shaders/ParticleUpdatergen_00000000.spv")
		.AddShader(vk::ShaderStageFlagBits::eMissKHR, "res/shaders/ParticleUpdatermiss_00000000.spv")
		.AddShader(vk::ShaderStageFlagBits::eClosestHitKHR, "res/shaders/ParticleUpdaterchit_00000000.spv")
		.AddShaderGroup(vk::RayTracingShaderGroupTypeKHR::eGeneral, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR)
		.AddShaderGroup(vk::RayTracingShaderGroupTypeKHR::eGeneral, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR)
		.AddShaderGroup(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup, VK_SHADER_UNUSED_KHR, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR);

	mp_ptcl_rt_descriptor_spec = std::make_shared<DescriptorSetSpec>();
	mp_ptcl_rt_descriptor_spec->AddDescriptor(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR)
		.AddDescriptor(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR) // Particles
		.AddDescriptor(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR) // Indices
		.AddDescriptor(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR) // Normals
		.AddDescriptor(4, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR) // RT instance buffer
		.AddDescriptor(5, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR) // Transforms
		.AddDescriptor(6, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR) // Transforms (previous frame)
		.AddDescriptor(7, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR) // Vertex positions
		.AddDescriptor(8, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR) // Cell key buffer
		.AddDescriptor(9, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR) // Cell key start idx buffer
		.AddDescriptor(10, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR) // Simulation results buffer
		.AddDescriptor(11, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR) // Simulation parameters buffer
		.GenDescriptorLayout();

	PipelineLayoutBuilder layout_builder{};
	layout_builder.AddDescriptorSet(0, p_scene->GetSystem<SceneInfoBufferSystem>()->descriptor_buffers[0].GetDescriptorSpec())
		.AddDescriptorSet(1, mp_ptcl_rt_descriptor_spec)
		.Build();

	m_ptcl_rt_pipeline.Init(rt_builder, layout_builder);

	// Contains all particle-specific descriptors, shared by all pipelines
	m_ptcl_compute_descriptor_spec = std::make_shared<DescriptorSetSpec>();
	m_ptcl_compute_descriptor_spec->AddDescriptor(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute)
		.AddDescriptor(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute)
		.AddDescriptor(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute)
		.AddDescriptor(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute)
		.GenDescriptorLayout();
}

void ParticleSystem::InitParticles() {
	InitBuffers();
	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_ptcl_compute_descriptor_buffers[i].SetDescriptorSpec(m_ptcl_compute_descriptor_spec);
		m_ptcl_compute_descriptor_buffers[i].CreateBuffer(1);

		m_ptcl_rt_descriptor_buffers[i].SetDescriptorSpec(mp_ptcl_rt_descriptor_spec);
		m_ptcl_rt_descriptor_buffers[i].CreateBuffer(1);

		auto ptcl_buf_get_info = m_ptcl_buffers[i].CreateDescriptorGetInfo();
		auto cell_buf_get_info = m_cell_key_buffers[i].CreateDescriptorGetInfo();
		auto cell_start_idx_get_info = m_cell_key_start_index_buffers[i].CreateDescriptorGetInfo();
		auto result_ptcl_buf_get_info = m_ptcl_result_buffers[i].CreateDescriptorGetInfo();

		m_ptcl_compute_descriptor_buffers[i].LinkResource(&m_cell_key_buffers[i], cell_buf_get_info, 0, 0);
		m_ptcl_compute_descriptor_buffers[i].LinkResource(&m_ptcl_buffers[i], ptcl_buf_get_info, 1, 0);
		m_ptcl_compute_descriptor_buffers[i].LinkResource(&m_cell_key_start_index_buffers[i], cell_start_idx_get_info, 2, 0);
		m_ptcl_compute_descriptor_buffers[i].LinkResource(&m_ptcl_result_buffers[i], result_ptcl_buf_get_info, 3, 0);

		auto mesh_buffers = AssetManager::Get().mesh_buffer_manager.GetMeshBuffers();
		auto index_buf_get_info = mesh_buffers.indices_buf.CreateDescriptorGetInfo();
		auto normal_buf_get_info = mesh_buffers.normal_buf.CreateDescriptorGetInfo();
		auto position_buf_get_info = mesh_buffers.position_buf.CreateDescriptorGetInfo();
		auto& tlas = p_scene->GetSystem<TlasSystem>()->GetTlas(i);
		auto tlas_get_info = tlas.CreateDescriptorGetInfo();
		auto* p_rt_buf_system = p_scene->GetSystem<RaytracingInstanceBufferSystem>();
		auto rt_instance_buf_get_info = p_rt_buf_system->GetStorageBuffer(i).CreateDescriptorGetInfo();
		auto* p_transform_buffer_system = p_scene->GetSystem<TransformBufferSystem>();
		auto transform_buf_get_info = p_transform_buffer_system->GetTransformStorageBuffer(i).CreateDescriptorGetInfo();
		auto transform_buf_prev_frame_get_info = p_transform_buffer_system->GetLastFramesTransformStorageBuffer(i).CreateDescriptorGetInfo();
		auto params_ubo_get_info = m_params_buffers[i].CreateDescriptorGetInfo();

		m_ptcl_rt_descriptor_buffers[i].LinkResource(&tlas, tlas_get_info, 0, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&m_ptcl_buffers[i], ptcl_buf_get_info, 1, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&mesh_buffers.indices_buf, index_buf_get_info, 2, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&mesh_buffers.normal_buf, normal_buf_get_info, 3, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&p_rt_buf_system->GetStorageBuffer(i), rt_instance_buf_get_info, 4, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&p_transform_buffer_system->GetTransformStorageBuffer(i), transform_buf_get_info, 5, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&p_transform_buffer_system->GetLastFramesTransformStorageBuffer(i), transform_buf_prev_frame_get_info, 6, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&mesh_buffers.position_buf, position_buf_get_info, 7, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&m_cell_key_buffers[i], cell_buf_get_info, 8, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&m_cell_key_start_index_buffers[i], cell_start_idx_get_info, 9, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&m_ptcl_result_buffers[i], result_ptcl_buf_get_info, 10, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&m_params_buffers[i], params_ubo_get_info, 11, 0);
	}

	InitializeSimulation();
}

void ParticleSystem::InitBuffers() {
	unsigned PARTICLE_DATA_SIZE = sizeof(glm::vec4) * 2 * m_num_particles;
	VkContext::GetLogicalDevice().GraphicsQueueWaitIdle();

	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
	m_ptcl_compute_descriptor_buffers[i].SetDescriptorSpec(m_ptcl_compute_descriptor_spec);
		if (m_ptcl_buffers[i].IsCreated()) {
			m_ptcl_buffers[i].DestroyBuffer();
			m_ptcl_result_buffers[i].DestroyBuffer();
			m_cell_key_buffers[i].DestroyBuffer();
			m_cell_key_start_index_buffers[i].DestroyBuffer();
			m_ptcl_buffers_prev_frame[i].DestroyBuffer();
			m_params_buffers[i].DestroyBuffer();
		}

		m_ptcl_buffers[i].CreateBuffer(PARTICLE_DATA_SIZE, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc |
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);

		m_ptcl_result_buffers[i].CreateBuffer(PARTICLE_DATA_SIZE, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc |
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);

		m_cell_key_buffers[i].CreateBuffer(m_num_particles * sizeof(uint32_t) * 2, vk::BufferUsageFlagBits::eStorageBuffer |
			vk::BufferUsageFlagBits::eShaderDeviceAddress);

		m_cell_key_start_index_buffers[i].CreateBuffer(m_num_particles * sizeof(uint32_t), vk::BufferUsageFlagBits::eStorageBuffer |
			vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst);

		m_ptcl_buffers_prev_frame[i].CreateBuffer(PARTICLE_DATA_SIZE, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc |
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);

		m_params_buffers[i].CreateBuffer(aligned_size(sizeof(PhysicsParamsUBO), 64), vk::BufferUsageFlagBits::eUniformBuffer |
			vk::BufferUsageFlagBits::eShaderDeviceAddress, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
	}

}

void ParticleSystem::SetNumParticles(uint32_t num_particles) {
	m_num_particles = glm::ceilMultiple(glm::max(num_particles, 1u), 64u);
	InitBuffers();
	InitializeSimulation();
}

void ParticleSystem::InitializeSimulation() {
	auto fif = VkContext::GetCurrentFIF();
	auto cmd = *m_cmd_buffers[fif].buf;
	VkContext::GetLogicalDevice().GraphicsQueueWaitIdle();
	cmd.reset();
	vk::CommandBufferBeginInfo begin_info{};
	SNK_CHECK_VK_RESULT(cmd.begin(begin_info));
	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipelines[ParticleComputeShader::INIT].GetPipeline());
	std::array<uint32_t, 2> db_indices = { 0, 1 };
	std::array<vk::DeviceSize, 2> db_offsets = { 0, 0 };

	auto binding_infos = util::array(p_scene->GetSystem<SceneInfoBufferSystem>()->descriptor_buffers[0].GetBindingInfo(),
		m_ptcl_compute_descriptor_buffers[0].GetBindingInfo());

	cmd.bindDescriptorBuffersEXT(binding_infos);
	cmd.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eCompute, m_compute_pipelines[ParticleComputeShader::INIT].pipeline_layout.GetPipelineLayout(), 0, db_indices, db_offsets);

	cmd.dispatch(m_num_particles, 1, 1);
	for (FrameInFlightIndex i = 1; i < MAX_FRAMES_IN_FLIGHT; i++) {
		CopyBuffer(m_ptcl_buffers[i - 1].buffer, m_ptcl_buffers[i].buffer, sizeof(glm::vec4) * 2 * m_num_particles, 0u, 0u, *m_cmd_buffers[fif].buf);
	}
	SNK_CHECK_VK_RESULT(cmd.end());

	vk::SubmitInfo submit_info{};
	submit_info.pCommandBuffers = &cmd;
	submit_info.commandBufferCount = 1;
	VkContext::GetLogicalDevice().SubmitGraphics(submit_info);
}

struct BitonicSortData {
	uint32_t group_width;
	uint32_t group_height;
	uint32_t step_idx;
};

void ParticleSystem::UpdateParticles() {
	SNK_ASSERT(m_ptcl_compute_descriptor_buffers[0].GetDescriptorSpec().lock());
	SNK_ASSERT(m_ptcl_compute_descriptor_buffers[1].GetDescriptorSpec().lock());
	if (!active)
		return;

	auto fif = VkContext::GetCurrentFIF();
	auto cmd = *m_cmd_buffers[fif].buf;

	for (int i = 0; i < 3; i++) {
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipelines[ParticleComputeShader::CELL_KEY_GENERATION].GetPipeline());
		std::array<uint32_t, 2> db_indices = { 0, 1 };
		std::array<vk::DeviceSize, 2> db_offsets = { 0, 0 };

		auto binding_infos = util::array(p_scene->GetSystem<SceneInfoBufferSystem>()->descriptor_buffers[fif].GetBindingInfo(),
			m_ptcl_compute_descriptor_buffers[fif].GetBindingInfo());
	
		// Fill cell key buffer
		cmd.bindDescriptorBuffersEXT(binding_infos);
		cmd.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eCompute, m_compute_pipelines[ParticleComputeShader::CELL_KEY_GENERATION].pipeline_layout.GetPipelineLayout(), 0, db_indices, db_offsets);
		cmd.dispatch((uint32_t)glm::ceil(m_num_particles / 128.f), 1, 1);
		m_cell_key_buffers[fif].MemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, cmd);

		// Sort cell keys 
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipelines[ParticleComputeShader::BITONIC_CELL_KEY_SORT].GetPipeline());
		cmd.bindDescriptorBuffersEXT(binding_infos);
		cmd.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eCompute, m_compute_pipelines[ParticleComputeShader::BITONIC_CELL_KEY_SORT].pipeline_layout.GetPipelineLayout(), 0, db_indices, db_offsets);

		BitonicSortData bitonic_sort_data;

		// Ty Sebastian Lague "Coding Adventure: Simulating Fluids"
		unsigned num_pairs = glm::ceilPowerOfTwo(m_num_particles) / 2;
		unsigned num_stages = glm::log2(num_pairs * 2u);

		for (unsigned stage_idx = 0; stage_idx < num_stages; stage_idx++) {
			for (unsigned step_idx = 0; step_idx < stage_idx + 1; step_idx++) {
				unsigned group_width = 1 << (stage_idx - step_idx);
				unsigned group_height = 2 * group_width - 1;

				bitonic_sort_data.group_width = group_width;
				bitonic_sort_data.group_height = group_height;
				bitonic_sort_data.step_idx = step_idx;
				cmd.pushConstants(m_compute_pipelines[ParticleComputeShader::BITONIC_CELL_KEY_SORT].pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(BitonicSortData), &bitonic_sort_data);

				cmd.dispatch((uint32_t)glm::ceil(num_pairs / 128.f), 1, 1);
				m_cell_key_buffers[fif].MemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, cmd);
			}
		}

		// Generate start indices for cell keys
		cmd.fillBuffer(m_cell_key_start_index_buffers[fif].buffer, 0, m_num_particles * sizeof(uint32_t), ~0u);
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipelines[ParticleComputeShader::CELL_KEY_START_IDX_GENERATION].GetPipeline());
		cmd.bindDescriptorBuffersEXT(binding_infos);
		cmd.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eCompute, m_compute_pipelines[ParticleComputeShader::CELL_KEY_START_IDX_GENERATION].pipeline_layout.GetPipelineLayout(), 0, db_indices, db_offsets);
		cmd.dispatch((uint32_t)glm::ceil(m_num_particles / 128.f), 1, 1);
		m_cell_key_start_index_buffers[fif].MemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eRayTracingShaderKHR, cmd);

		// Collision handling
		auto& sbt = m_ptcl_rt_pipeline.GetSBT();
		auto binding_infos_rt = util::array(binding_infos[0], m_ptcl_rt_descriptor_buffers[fif].GetBindingInfo());
		cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, m_ptcl_rt_pipeline.GetPipeline());
		cmd.bindDescriptorBuffersEXT(binding_infos_rt);
		cmd.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eRayTracingKHR, m_ptcl_rt_pipeline.GetPipelineLayout(), 0, db_indices, db_offsets);
		cmd.traceRaysKHR(sbt.address_regions.rgen, sbt.address_regions.rmiss, sbt.address_regions.rhit, sbt.address_regions.callable, m_num_particles, 1, 1);
		m_ptcl_result_buffers[fif].MemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead, vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::PipelineStageFlagBits::eTransfer, cmd);
		CopyBuffer(m_ptcl_result_buffers[fif].buffer, m_ptcl_buffers[fif].buffer, m_num_particles * sizeof(glm::vec4) * 2, 0, 0, cmd);
		m_ptcl_buffers[fif].MemoryBarrier(vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, cmd);

	}
	
	SNK_CHECK_VK_RESULT(cmd.end());
	vk::SubmitInfo submit_info{};
	submit_info.pCommandBuffers = &cmd;
	submit_info.commandBufferCount = 1;
	VkContext::GetLogicalDevice().SubmitGraphics(submit_info);
}