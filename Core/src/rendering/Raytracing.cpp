#include "pch/pch.h"
#include "rendering/Raytracing.h"
#include "scene/RaytracingBufferSystem.h"
#include "components/Components.h"
#include "scene/LightBufferSystem.h"
#include "rendering/RenderCommon.h"
#include "scene/TlasSystem.h"
#include "scene/SceneInfoBufferSystem.h"
#include "scene/TransformBufferSystem.h"

using namespace SNAKE;

//void RT::UpdateTLAS() {
	//auto& logical_device = VkContext::GetLogicalDevice();

	//static float t = 0.f;
	//t += 0.01f;

	//VkTransformMatrixKHR instance_transform = {
	//	1.f, 0.f, 0.f, 10*sinf(t * 0.001f),
	//	0.f, 1.f, 0.f, 0.f,
	//	0.f, 0.f, 1.f, 0.f
	//};

	//vk::AccelerationStructureInstanceKHR instance{};
	//instance.transform = instance_transform;
	//instance.instanceCustomIndex = 0;
	//instance.mask = 0xFF;
	//instance.instanceShaderBindingTableRecordOffset = 0;
	//instance.flags = static_cast<VkGeometryInstanceFlagBitsKHR>(vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable);
	//instance.accelerationStructureReference = blas_handle; // Handle of BLAS for the sphere mesh
	//std::vector<vk::AccelerationStructureInstanceKHR> instances = { instance };

	//auto* p_instance_data = instance_buffer.Map();
	//memcpy(p_instance_data, instances.data(), sizeof(vk::AccelerationStructureInstanceKHR) * instances.size());

	//vk::AccelerationStructureGeometryKHR as_geom_info{};
	//as_geom_info.flags = vk::GeometryFlagBitsKHR::eOpaque;
	//as_geom_info.geometryType = vk::GeometryTypeKHR::eInstances;
	//as_geom_info.geometry.instances.arrayOfPointers = false;
	//as_geom_info.geometry.instances.data = instance_buffer.GetDeviceAddress();
	//as_geom_info.geometry.instances.sType = vk::StructureType::eAccelerationStructureGeometryInstancesDataKHR;

	//vk::AccelerationStructureBuildGeometryInfoKHR as_build_geom_info{};
	//as_build_geom_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
	//as_build_geom_info.mode = vk::BuildAccelerationStructureModeKHR::eUpdate;
	//as_build_geom_info.dstAccelerationStructure = *p_tlas;
	//as_build_geom_info.srcAccelerationStructure = *p_tlas;
	//as_build_geom_info.geometryCount = 1;
	//as_build_geom_info.pGeometries = &as_geom_info;
	//as_build_geom_info.scratchData.deviceAddress = tlas_scratch_buf.GetDeviceAddress();
	//as_build_geom_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;

	//vk::AccelerationStructureBuildRangeInfoKHR as_build_range_info{};
	//as_build_range_info.primitiveCount = instances.size();
	//as_build_range_info.primitiveOffset = 0;
	//as_build_range_info.firstVertex = 0;
	//as_build_range_info.transformOffset = 0;

	//auto cmd = BeginSingleTimeCommands();
	//cmd->buildAccelerationStructuresKHR(as_build_geom_info, &as_build_range_info);
	//EndSingleTimeCommands(*cmd);

	//vk::AccelerationStructureDeviceAddressInfoKHR device_address_info{};
	//device_address_info.accelerationStructure = *p_tlas;
	//tlas_handle = logical_device.device->getAccelerationStructureAddressKHR(device_address_info);
	//SNK_ASSERT(tlas_handle);
//}

void RT::InitDescriptorBuffers(Image2D& output_image, Scene& scene, RaytracingResources& output_resources, TlasSystem& tlas_system) {
	rt_descriptor_set_spec = std::make_shared<DescriptorSetSpec>();
	rt_descriptor_set_spec->AddDescriptor(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eAll) // TLAS
		.AddDescriptor(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR) // Output image
		.AddDescriptor(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll) // Vertex position buffer
		.AddDescriptor(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll) // Vertex index buffer
		.AddDescriptor(4, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll) // Vertex normal buffer
		.AddDescriptor(5, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll) // Vertex tex coord buffer
		.AddDescriptor(6, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll) // Vertex tangent buffer
		.AddDescriptor(7, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll) // Raytracing instance buffer
		.AddDescriptor(8, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR)  // Light buffer
		.AddDescriptor(9, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR) // Input gbuffer albedo sampler
		.AddDescriptor(10, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR) // Input gbuffer normal sampler
		.AddDescriptor(11, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR) // Input gbuffer depth sampler
		.AddDescriptor(12, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR) // Input gbuffer rma sampler
		.AddDescriptor(13, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR) // Input gbuffer mat flag sampler
		.AddDescriptor(14, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll) // Transforms
		.AddDescriptor(15, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll) // Transforms (previous frame)
		.AddDescriptor(16, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAll) // Raytracing emissive instance idx buffer
		.AddDescriptor(17, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eRaygenKHR) // Pixel reservoir buffer
		.AddDescriptor(18, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR) // Input gbuffer pixel motion sampler
		.AddDescriptor(19, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR) // Input gbuffer prev frame depth sampler
		.AddDescriptor(20, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR) // Input gbuffer prev frame normal motion sampler
		.GenDescriptorLayout();

	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		rt_descriptor_buffers[i].SetDescriptorSpec(rt_descriptor_set_spec);
		rt_descriptor_buffers[i].CreateBuffer(1);
		
		auto mesh_buffers = AssetManager::Get().mesh_buffer_manager.GetMeshBuffers();

		auto get_info_tlas = tlas_system.GetTlas(i).CreateDescriptorGetInfo();
		auto get_info_image = output_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage);
		auto get_info_position_buf = mesh_buffers.position_buf.CreateDescriptorGetInfo();
		auto get_info_index_buf = mesh_buffers.indices_buf.CreateDescriptorGetInfo();
		auto get_info_normal_buf = mesh_buffers.normal_buf.CreateDescriptorGetInfo();
		auto get_info_tex_coord_buf = mesh_buffers.tex_coord_buf.CreateDescriptorGetInfo();
		auto get_info_tangent_buf = mesh_buffers.tangent_buf.CreateDescriptorGetInfo();
		auto get_info_instance_buf = scene.GetSystem<RaytracingInstanceBufferSystem>()->GetInstanceStorageBuffer(i).CreateDescriptorGetInfo();
		auto get_info_emissive_idx_buf = scene.GetSystem<RaytracingInstanceBufferSystem>()->GetEmissiveIdxStorageBuffer(i).CreateDescriptorGetInfo();
		auto get_info_light_buf = scene.GetSystem<LightBufferSystem>()->light_ssbos[i].CreateDescriptorGetInfo();
		auto get_info_albedo_gbuffer_image = output_resources.albedo_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler);
		auto get_info_normal_gbuffer_image = output_resources.normal_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler);
		auto get_info_depth_gbuffer_image = output_resources.depth_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler);
		auto get_info_rma_gbuffer_image = output_resources.rma_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler);
		auto get_info_mat_flag_image = output_resources.mat_flag_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler);
		auto* p_transform_buffer_sys = scene.GetSystem<TransformBufferSystem>();
		auto get_info_transform_buffer = p_transform_buffer_sys->GetTransformStorageBuffer(i).CreateDescriptorGetInfo();
		auto get_info_transform_buffer_prev_frame = p_transform_buffer_sys->GetLastFramesTransformStorageBuffer(i).CreateDescriptorGetInfo();
		auto get_info_reservoir_buf = output_resources.reservoir_buffer.CreateDescriptorGetInfo();
		auto get_info_pixel_motion_image = output_resources.pixel_motion_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler);
		auto get_info_prev_depth_image = output_resources.prev_frame_depth_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler);
		auto get_info_prev_normal_image = output_resources.prev_frame_normal_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler);

		rt_descriptor_buffers[i].LinkResource(&tlas_system.GetTlas(i), get_info_tlas, 0, 0);
		rt_descriptor_buffers[i].LinkResource(&output_image, get_info_image, 1, 0);
		rt_descriptor_buffers[i].LinkResource(&mesh_buffers.position_buf, get_info_position_buf, 2, 0);
		rt_descriptor_buffers[i].LinkResource(&mesh_buffers.indices_buf, get_info_index_buf, 3, 0);
		rt_descriptor_buffers[i].LinkResource(&mesh_buffers.normal_buf, get_info_normal_buf, 4, 0);
		rt_descriptor_buffers[i].LinkResource(&mesh_buffers.tex_coord_buf, get_info_tex_coord_buf, 5, 0);
		rt_descriptor_buffers[i].LinkResource(&mesh_buffers.tangent_buf, get_info_tangent_buf, 6, 0);
		rt_descriptor_buffers[i].LinkResource(&scene.GetSystem<RaytracingInstanceBufferSystem>()->GetInstanceStorageBuffer(i), get_info_instance_buf, 7, 0);
		rt_descriptor_buffers[i].LinkResource(&scene.GetSystem<LightBufferSystem>()->light_ssbos[i], get_info_light_buf, 8, 0);
		rt_descriptor_buffers[i].LinkResource(&output_resources.albedo_image, get_info_albedo_gbuffer_image, 9, 0);
		rt_descriptor_buffers[i].LinkResource(&output_resources.normal_image, get_info_normal_gbuffer_image, 10, 0);
		rt_descriptor_buffers[i].LinkResource(&output_resources.depth_image, get_info_depth_gbuffer_image, 11, 0);
		rt_descriptor_buffers[i].LinkResource(&output_resources.rma_image, get_info_rma_gbuffer_image, 12, 0);
		rt_descriptor_buffers[i].LinkResource(&output_resources.mat_flag_image, get_info_mat_flag_image, 13, 0);
		rt_descriptor_buffers[i].LinkResource(&p_transform_buffer_sys->GetTransformStorageBuffer(i), get_info_transform_buffer, 14, 0);
		rt_descriptor_buffers[i].LinkResource(&p_transform_buffer_sys->GetLastFramesTransformStorageBuffer(i), get_info_transform_buffer_prev_frame, 15, 0);
		rt_descriptor_buffers[i].LinkResource(&scene.GetSystem<RaytracingInstanceBufferSystem>()->GetEmissiveIdxStorageBuffer(i), get_info_emissive_idx_buf, 16, 0);
		rt_descriptor_buffers[i].LinkResource(&output_resources.reservoir_buffer, get_info_reservoir_buf, 17, 0);
		rt_descriptor_buffers[i].LinkResource(&output_resources.pixel_motion_image, get_info_pixel_motion_image, 18, 0);
		rt_descriptor_buffers[i].LinkResource(&output_resources.prev_frame_depth_image, get_info_prev_depth_image, 19, 0);
		rt_descriptor_buffers[i].LinkResource(&output_resources.prev_frame_normal_image, get_info_prev_normal_image, 20, 0);
	}
}

void RT::InitPipeline(std::weak_ptr<const DescriptorSetSpec> common_ubo_set) {
	RtPipelineBuilder pipeline_builder{};
	pipeline_builder.AddShader(vk::ShaderStageFlagBits::eRaygenKHR, "res/shaders/RayGenrgen_00000000.spv")
		.AddShader(vk::ShaderStageFlagBits::eMissKHR, "res/shaders/RayMissrmiss_00000000.spv")
		.AddShader(vk::ShaderStageFlagBits::eMissKHR, "res/shaders/Shadowrmiss_00000000.spv")
		.AddShader(vk::ShaderStageFlagBits::eClosestHitKHR, "res/shaders/RayClosestHitrchit_00000000.spv")
		.AddShaderGroup(vk::RayTracingShaderGroupTypeKHR::eGeneral, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR)
		.AddShaderGroup(vk::RayTracingShaderGroupTypeKHR::eGeneral, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR)
		.AddShaderGroup(vk::RayTracingShaderGroupTypeKHR::eGeneral, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR)
		.AddShaderGroup(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup, VK_SHADER_UNUSED_KHR, 3, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR);
		
	PipelineLayoutBuilder layout_builder{};
	layout_builder.AddDescriptorSet(0, common_ubo_set)
		.AddDescriptorSet(1, rt_descriptor_buffers[0].GetDescriptorSpec())
		.AddDescriptorSet(2, AssetManager::GetGlobalTexMatBufDescriptorSetSpec(0))
		.AddPushConstant(0, sizeof(bool), vk::ShaderStageFlagBits::eRaygenKHR)
		.Build();

	pipeline.Init(pipeline_builder, layout_builder);
}

void SBT::Init(vk::Pipeline pipeline, const RtPipelineBuilder& builder) {
	auto& ray_pipeline_properties = VkContext::GetPhysicalDevice().ray_properties;
	auto sbt_handle_size = ray_pipeline_properties.shaderGroupHandleSize;
	auto sbt_handle_alignment = ray_pipeline_properties.shaderGroupHandleAlignment;
	m_sbt_aligned_handle_size = (uint32_t)aligned_size(sbt_handle_size, sbt_handle_alignment);

	address_regions.rgen.stride = aligned_size(m_sbt_aligned_handle_size, ray_pipeline_properties.shaderGroupBaseAlignment);
	address_regions.rgen.size = address_regions.rgen.stride; // For rgen, size must be same as stride

	address_regions.rmiss.stride = m_sbt_aligned_handle_size;
	address_regions.rmiss.size = aligned_size(builder.GetShaderCount(vk::ShaderStageFlagBits::eMissKHR) * m_sbt_aligned_handle_size, ray_pipeline_properties.shaderGroupBaseAlignment);

	address_regions.rhit.stride = m_sbt_aligned_handle_size;
	address_regions.rhit.size = aligned_size(builder.GetShaderCount(vk::ShaderStageFlagBits::eClosestHitKHR) * m_sbt_aligned_handle_size, ray_pipeline_properties.shaderGroupBaseAlignment);

	auto sbt_size = address_regions.rgen.size + address_regions.rmiss.size + address_regions.rhit.size + address_regions.callable.size;

	std::vector<std::byte> handles(sbt_size);
	auto res = VkContext::GetLogicalDevice().device->getRayTracingShaderGroupHandlesKHR(pipeline, 0u, (uint32_t)builder.shader_groups.size(), (size_t)sbt_size, handles.data());
	SNK_CHECK_VK_RESULT(res);

	sbt_buffer.CreateBuffer(sbt_size, 
		vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

	auto GetHandle = [&](int i) { return handles.data() + i * sbt_handle_size; };

	std::byte* p_sbt_buf = reinterpret_cast<std::byte*>(sbt_buffer.Map());
	std::byte* p_data = nullptr;
	uint32_t handle_idx = 0;
	p_data = p_sbt_buf;

	// Raygen
	memcpy(p_data, GetHandle(handle_idx++), sbt_handle_size);

	// Miss
	p_data = p_sbt_buf + address_regions.rgen.size;
	for (uint32_t i = 0; i < builder.GetShaderCount(vk::ShaderStageFlagBits::eMissKHR); i++) {
		memcpy(p_data, GetHandle(handle_idx++), sbt_handle_size);
		p_data += address_regions.rmiss.stride;
	}

	// Hit
	p_data = p_sbt_buf + address_regions.rgen.size + address_regions.rmiss.size;
	for (uint32_t i = 0; i < builder.GetShaderCount(vk::ShaderStageFlagBits::eClosestHitKHR); i++) {
		memcpy(p_data, GetHandle(handle_idx++), sbt_handle_size);
		p_data += address_regions.rhit.stride;
	}
	sbt_buffer.Unmap();

	address_regions.rgen.deviceAddress = sbt_buffer.GetDeviceAddress();
	address_regions.rmiss.deviceAddress = sbt_buffer.GetDeviceAddress() + address_regions.rgen.size;
	address_regions.rhit.deviceAddress = sbt_buffer.GetDeviceAddress() + address_regions.rgen.size + address_regions.rmiss.size;
}


void RT::RecordRenderCmdBuf(vk::CommandBuffer cmd, Image2D& output_image, Scene& scene, RaytracingResources& output_resources) {
	FrameInFlightIndex fif = VkContext::GetCurrentFIF();

	output_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
		vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderWrite, vk::PipelineStageFlagBits::eRayTracingShaderKHR,
		vk::PipelineStageFlagBits::eRayTracingShaderKHR, 0, 1, cmd);

	cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, pipeline.GetPipeline());
	std::array<uint32_t, 3> db_indices = { 0, 1, 2 };
	std::array<vk::DeviceSize, 3> db_offsets = { 0, 0, 0 };

	auto binding_infos = util::array(
		scene.GetSystem<SceneInfoBufferSystem>()->descriptor_buffers[fif].GetBindingInfo(),
		rt_descriptor_buffers[fif].GetBindingInfo(), 
		AssetManager::GetGlobalTexMatBufBindingInfo(fif)
		);

	cmd.bindDescriptorBuffersEXT(binding_infos);
	cmd.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eRayTracingKHR, pipeline.GetPipelineLayout(), 0, db_indices, db_offsets);

	auto& spec = output_image.GetSpec();
	auto& sbt = pipeline.GetSBT();

	uint32_t initial_reservoir_pass = 1;
	cmd.pushConstants(pipeline.GetPipelineLayout(), vk::ShaderStageFlagBits::eRaygenKHR, 0u, sizeof(uint32_t), &initial_reservoir_pass);
	cmd.traceRaysKHR(sbt.address_regions.rgen, sbt.address_regions.rmiss, sbt.address_regions.rhit, sbt.address_regions.callable, spec.size.x, spec.size.y, 1);

	output_resources.reservoir_buffer.MemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead,
		vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::PipelineStageFlagBits::eRayTracingShaderKHR, cmd);

	initial_reservoir_pass = 0;
	cmd.pushConstants(pipeline.GetPipelineLayout(), vk::ShaderStageFlagBits::eRaygenKHR, 0u, sizeof(uint32_t), &initial_reservoir_pass);
	cmd.traceRaysKHR(sbt.address_regions.rgen, sbt.address_regions.rmiss, sbt.address_regions.rhit, sbt.address_regions.callable, spec.size.x, spec.size.y, 1);
	//output_image.TransitionImageLayout(vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
		//vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eRayTracingShaderKHR,
		//vk::PipelineStageFlagBits::eAllCommands, 0, 1, cmd);
}

void RT::Init(Scene& scene, Image2D& output_image, std::weak_ptr<const DescriptorSetSpec> common_ubo_set, RaytracingResources& resources) {
	if (auto* p_tlas_system = scene.GetSystem<TlasSystem>(); p_tlas_system) {
		InitDescriptorBuffers(output_image, scene, resources, *p_tlas_system);
	} else {
		SNK_CORE_ERROR("RT::Init failed, tried to initialize from a scene with no TlasSystem attached");
		return;
	}

	InitPipeline(common_ubo_set);
}

