#include "pch/pch.h"
#include "assets/AssetManager.h"
#include "core/VkCommon.h"
#include "scene/ParticleSystem.h"
#include "scene/SceneInfoBufferSystem.h"
#include "scene/Scene.h"
#include "scene/TlasSystem.h"
#include "scene/RaytracingBufferSystem.h"
#include "scene/TransformBufferSystem.h"

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

		uint8_t old_idx = (uint8_t)glm::min(fif - 1u, MAX_FRAMES_IN_FLIGHT - 1u);
		CopyBuffer(m_ptcl_buffers[old_idx].buffer, m_ptcl_buffers_prev_frame[fif].buffer, PARTICLE_DATA_SIZE);
		CopyBuffer(m_ptcl_buffers[old_idx].buffer, m_ptcl_buffers[fif].buffer, PARTICLE_DATA_SIZE);


		UpdateParticles();
	};

	EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);
}

void ParticleSystem::OnSystemRemove() {
	EventManagerG::DeregisterListener(m_frame_start_listener);
}

void ParticleSystem::InitPipelines() {
	m_compute_pipelines[ParticleComputeShader::INIT].Init("res/shaders/Particlecomp_10000000.spv");
	m_compute_pipelines[ParticleComputeShader::UPDATE].Init("res/shaders/Particlecomp_01000000.spv");

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
		.GenDescriptorLayout();

	PipelineLayoutBuilder layout_builder{};
	layout_builder.AddDescriptorSet(0, p_scene->GetSystem<SceneInfoBufferSystem>()->descriptor_buffers[0].GetDescriptorSpec())
		.AddDescriptorSet(1, mp_ptcl_rt_descriptor_spec)
		.Build();

	m_ptcl_rt_pipeline.Init(rt_builder, layout_builder);

	// Contains all particle-specific descriptors, shared by all pipelines
	m_ptcl_compute_descriptor_spec = m_compute_pipelines[ParticleComputeShader::INIT].pipeline_layout.GetDescriptorSetLayout(1);
}

void ParticleSystem::InitParticles() {
	unsigned PARTICLE_DATA_SIZE = sizeof(glm::vec4) * 2 * m_num_particles;

	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_ptcl_buffers[i].CreateBuffer(PARTICLE_DATA_SIZE, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc |
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);

		m_ptcl_buffers_prev_frame[i].CreateBuffer(PARTICLE_DATA_SIZE, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc |
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);

		m_ptcl_compute_descriptor_buffers[i].SetDescriptorSpec(m_ptcl_compute_descriptor_spec);
		m_ptcl_compute_descriptor_buffers[i].CreateBuffer(1);

		m_ptcl_rt_descriptor_buffers[i].SetDescriptorSpec(mp_ptcl_rt_descriptor_spec);
		m_ptcl_rt_descriptor_buffers[i].CreateBuffer(1);

		auto ptcl_buf_get_info = m_ptcl_buffers[i].CreateDescriptorGetInfo();
		m_ptcl_compute_descriptor_buffers[i].LinkResource(&m_ptcl_buffers[i], ptcl_buf_get_info, 0, 0);

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

		m_ptcl_rt_descriptor_buffers[i].LinkResource(&tlas, tlas_get_info, 0, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&m_ptcl_buffers[i], ptcl_buf_get_info, 1, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&mesh_buffers.indices_buf, index_buf_get_info, 2, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&mesh_buffers.normal_buf, normal_buf_get_info, 3, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&p_rt_buf_system->GetStorageBuffer(i), rt_instance_buf_get_info, 4, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&p_transform_buffer_system->GetTransformStorageBuffer(i), transform_buf_get_info, 5, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&p_transform_buffer_system->GetLastFramesTransformStorageBuffer(i), transform_buf_prev_frame_get_info, 6, 0);
		m_ptcl_rt_descriptor_buffers[i].LinkResource(&mesh_buffers.position_buf, position_buf_get_info, 7, 0);
	}

	// Intialize positions through compute shader
	{
		auto fif = VkContext::GetCurrentFIF();
		auto cmd = *m_cmd_buffers[fif].buf;
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
		SNK_CHECK_VK_RESULT(cmd.end());

		vk::SubmitInfo submit_info{};
		submit_info.pCommandBuffers = &cmd;
		submit_info.commandBufferCount = 1;
		VkContext::GetLogicalDevice().SubmitGraphics(submit_info);

		CopyBuffer(m_ptcl_buffers[0].buffer, m_ptcl_buffers[1].buffer, PARTICLE_DATA_SIZE);
	}
}

void ParticleSystem::UpdateParticles() {
	if (!active)
		return;

	auto fif = VkContext::GetCurrentFIF();
	auto cmd = *m_cmd_buffers[fif].buf;
	cmd.reset();

	vk::CommandBufferBeginInfo begin_info{};
	SNK_CHECK_VK_RESULT(cmd.begin(begin_info));
	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipelines[ParticleComputeShader::UPDATE].GetPipeline());
	std::array<uint32_t, 2> db_indices = { 0, 1 };
	std::array<vk::DeviceSize, 2> db_offsets = { 0, 0 };

	auto binding_infos = util::array(p_scene->GetSystem<SceneInfoBufferSystem>()->descriptor_buffers[fif].GetBindingInfo(),
		m_ptcl_compute_descriptor_buffers[fif].GetBindingInfo());

	cmd.bindDescriptorBuffersEXT(binding_infos);
	cmd.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eCompute, m_compute_pipelines[ParticleComputeShader::UPDATE].pipeline_layout.GetPipelineLayout(), 0, db_indices, db_offsets);

	cmd.dispatch(m_num_particles, 1, 1);

	cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, m_ptcl_rt_pipeline.GetPipeline());
	auto binding_infos_rt = util::array(binding_infos[0], m_ptcl_rt_descriptor_buffers[fif].GetBindingInfo());
	cmd.bindDescriptorBuffersEXT(binding_infos_rt);
	cmd.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eRayTracingKHR, m_ptcl_rt_pipeline.GetPipelineLayout(), 0, db_indices, db_offsets);

	auto& sbt = m_ptcl_rt_pipeline.GetSBT();
	cmd.traceRaysKHR(sbt.address_regions.rgen, sbt.address_regions.rmiss, sbt.address_regions.rhit, sbt.address_regions.callable, m_num_particles, 1, 1);

	SNK_CHECK_VK_RESULT(cmd.end());
	
	vk::SubmitInfo submit_info{};
	submit_info.pCommandBuffers = &cmd;
	submit_info.commandBufferCount = 1;
	VkContext::GetLogicalDevice().SubmitGraphics(submit_info);
}