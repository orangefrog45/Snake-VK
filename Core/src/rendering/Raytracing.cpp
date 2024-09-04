#include "pch/pch.h"
#include "rendering/Raytracing.h"
#include "scene/TransformBufferSystem.h"

using namespace SNAKE;

void RT::InitTLAS(Scene& scene, FrameInFlightIndex idx) {

	if (tlas_arr[idx]) {
		tlas_arr[idx].release();
		tlas_buf_arr[idx].DestroyBuffer();
		instance_buffers[idx].DestroyBuffer();
	}

	auto& logical_device = VkContext::GetLogicalDevice();

	std::vector<vk::AccelerationStructureInstanceKHR> instances;

	auto& reg = scene.GetRegistry();
	auto mesh_view = reg.view<StaticMeshComponent>();
	if (mesh_view.empty())
		return;

	for (auto [entity, mesh] : mesh_view.each()) {
		instances.push_back(mesh.mesh_asset->data->p_blas->GenerateInstance(reg.get<TransformComponent>(entity)));
	}

	// Buffer to hold instance data e.g transform, blas
	instance_buffers[idx].CreateBuffer(sizeof(vk::AccelerationStructureInstanceKHR) * instances.size(), vk::BufferUsageFlagBits::eShaderDeviceAddress |
		vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
	auto* p_instance_data = instance_buffers[idx].Map();
	memcpy(p_instance_data, instances.data(), sizeof(vk::AccelerationStructureInstanceKHR) * instances.size());

	vk::AccelerationStructureGeometryKHR as_geom_info{};
	as_geom_info.flags = vk::GeometryFlagBitsKHR::eOpaque;
	as_geom_info.geometryType = vk::GeometryTypeKHR::eInstances;
	as_geom_info.geometry.instances.arrayOfPointers = false;
	as_geom_info.geometry.instances.data = instance_buffers[idx].GetDeviceAddress();
	as_geom_info.geometry.instances.sType = vk::StructureType::eAccelerationStructureGeometryInstancesDataKHR;

	vk::AccelerationStructureBuildGeometryInfoKHR as_build_size_geom_info{};
	as_build_size_geom_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
	as_build_size_geom_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
	as_build_size_geom_info.geometryCount = 1;
	as_build_size_geom_info.pGeometries = &as_geom_info;

	vk::AccelerationStructureBuildSizesInfoKHR as_build_size_info = logical_device.device->getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice, as_build_size_geom_info, 3800);

	tlas_buf_arr[idx].CreateBuffer(as_build_size_info.accelerationStructureSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);

	vk::AccelerationStructureCreateInfoKHR as_info{};
	as_info.buffer = tlas_buf_arr[idx].buffer;
	as_info.size = as_build_size_info.accelerationStructureSize;
	as_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;

	auto [res, value] = logical_device.device->createAccelerationStructureKHRUnique(as_info);
	SNK_CHECK_VK_RESULT(res);
	tlas_arr[idx] = std::move(value);

	S_VkBuffer tlas_scratch_buf;
	tlas_scratch_buf.CreateBuffer(as_build_size_info.buildScratchSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);

	vk::AccelerationStructureBuildGeometryInfoKHR as_build_geom_info{};
	as_build_geom_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
	as_build_geom_info.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
	as_build_geom_info.dstAccelerationStructure = *tlas_arr[idx];
	as_build_geom_info.geometryCount = 1;
	as_build_geom_info.pGeometries = &as_geom_info;
	as_build_geom_info.scratchData.deviceAddress = tlas_scratch_buf.GetDeviceAddress();
	as_build_geom_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;

	vk::AccelerationStructureBuildRangeInfoKHR as_build_range_info{};
	as_build_range_info.primitiveCount = instances.size();
	as_build_range_info.primitiveOffset = 0;
	as_build_range_info.firstVertex = 0;
	as_build_range_info.transformOffset = 0;

	auto cmd = BeginSingleTimeCommands();
	cmd->buildAccelerationStructuresKHR(as_build_geom_info, &as_build_range_info);
	EndSingleTimeCommands(*cmd);

	vk::AccelerationStructureDeviceAddressInfoKHR device_address_info{};
	device_address_info.accelerationStructure = *tlas_arr[idx];
	tlas_handles[idx] = logical_device.device->getAccelerationStructureAddressKHR(device_address_info);
	SNK_ASSERT(tlas_handles[idx]);
}


void RT::UpdateTLAS() {
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
}

void RT::InitDescriptorBuffers(Image2D& output_image, Scene& scene) {
	rt_descriptor_set_spec = std::make_shared<DescriptorSetSpec>();
	rt_descriptor_set_spec->AddDescriptor(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eAll)
		.AddDescriptor(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR)
		.AddDescriptor(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR)
		.AddDescriptor(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR)
		.AddDescriptor(4, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR)
		.AddDescriptor(5, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR)
		.GenDescriptorLayout();

	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		rt_descriptor_buffers[i].SetDescriptorSpec(rt_descriptor_set_spec);

		rt_descriptor_buffers[i].CreateBuffer(1);
		vk::DescriptorGetInfoEXT get_info{};
		get_info.type = vk::DescriptorType::eAccelerationStructureKHR;
		get_info.data.accelerationStructure = tlas_handles[i];

		vk::DescriptorImageInfo image_info{};
		image_info.imageLayout = vk::ImageLayout::eGeneral;
		image_info.imageView = output_image.GetImageView();
		image_info.sampler = output_image.GetSampler();

		vk::DescriptorGetInfoEXT get_info_image{};
		get_info_image.type = vk::DescriptorType::eStorageImage;
		get_info_image.data.pStorageImage = &image_info;

		auto mesh_buffers = AssetManager::Get().mesh_buffer_manager.GetMeshBuffers();
		auto [get_info_position_buf, a] = mesh_buffers.position_buf.CreateDescriptorGetInfo();
		auto [get_info_index_buf, b] = mesh_buffers.indices_buf.CreateDescriptorGetInfo();
		auto [get_info_normal_buf, c] = mesh_buffers.normal_buf.CreateDescriptorGetInfo();
		auto [get_info_transform_buf, d] = scene.GetSystem<TransformBufferSystem>()->GetTransformStorageBuffer(i).CreateDescriptorGetInfo();

		rt_descriptor_buffers[i].LinkResource(&get_info, 0, 0);
		rt_descriptor_buffers[i].LinkResource(&get_info_image, 1, 0);
		rt_descriptor_buffers[i].LinkResource(&get_info_position_buf, 2, 0);
		rt_descriptor_buffers[i].LinkResource(&get_info_index_buf, 3, 0);
		rt_descriptor_buffers[i].LinkResource(&get_info_normal_buf, 4, 0);
		rt_descriptor_buffers[i].LinkResource(&get_info_transform_buf, 5, 0);
	}
}

void RT::InitPipeline(vk::DescriptorSetLayout common_ubo_set_layout) {
	{
		auto layouts = util::array(common_ubo_set_layout, rt_descriptor_buffers[0].GetDescriptorSpec()->GetLayout());

		vk::PipelineLayoutCreateInfo pipeline_layout_info{};
		pipeline_layout_info.setLayoutCount = layouts.size();
		pipeline_layout_info.pSetLayouts = layouts.data();
		auto [res, value] = VkContext::GetLogicalDevice().device->createPipelineLayoutUnique(pipeline_layout_info);
		SNK_CHECK_VK_RESULT(res);
		rt_pipeline_layout = std::move(value);
	}

	auto ray_gen_module = ShaderLibrary::CreateShaderModule("res/shaders/RayGenrgen_00000000.spv");
	auto ray_chit_module = ShaderLibrary::CreateShaderModule("res/shaders/RayClosestHitrchit_00000000.spv");
	auto ray_miss_module = ShaderLibrary::CreateShaderModule("res/shaders/RayMissrmiss_00000000.spv");

	vk::PipelineShaderStageCreateInfo ray_gen_shader_stage_info{};
	ray_gen_shader_stage_info.stage = vk::ShaderStageFlagBits::eRaygenKHR;
	ray_gen_shader_stage_info.module = *ray_gen_module;
	ray_gen_shader_stage_info.pName = "main";

	vk::PipelineShaderStageCreateInfo chit_shader_stage_info{};
	chit_shader_stage_info.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
	chit_shader_stage_info.module = *ray_chit_module;
	chit_shader_stage_info.pName = "main";

	vk::PipelineShaderStageCreateInfo miss_shader_stage_info{};
	miss_shader_stage_info.stage = vk::ShaderStageFlagBits::eMissKHR;
	miss_shader_stage_info.module = *ray_miss_module;
	miss_shader_stage_info.pName = "main";

	vk::RayTracingShaderGroupCreateInfoKHR ray_gen_group{};
	ray_gen_group.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
	ray_gen_group.generalShader = 0;
	ray_gen_group.closestHitShader = VK_SHADER_UNUSED_KHR;
	ray_gen_group.anyHitShader = VK_SHADER_UNUSED_KHR;
	ray_gen_group.intersectionShader = VK_SHADER_UNUSED_KHR;

	vk::RayTracingShaderGroupCreateInfoKHR ray_miss_group{};
	ray_miss_group.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
	ray_miss_group.generalShader = 1;
	ray_miss_group.closestHitShader = VK_SHADER_UNUSED_KHR;
	ray_miss_group.anyHitShader = VK_SHADER_UNUSED_KHR;
	ray_miss_group.intersectionShader = VK_SHADER_UNUSED_KHR;

	vk::RayTracingShaderGroupCreateInfoKHR ray_chit_group{};
	ray_chit_group.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
	ray_chit_group.generalShader = VK_SHADER_UNUSED_KHR;
	ray_chit_group.closestHitShader = 2;
	ray_chit_group.anyHitShader = VK_SHADER_UNUSED_KHR;
	ray_chit_group.intersectionShader = VK_SHADER_UNUSED_KHR;
	
	std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shader_groups = { ray_gen_group, ray_miss_group, ray_chit_group };
	std::vector<vk::PipelineShaderStageCreateInfo> shader_stages = { ray_gen_shader_stage_info, miss_shader_stage_info, chit_shader_stage_info };
	vk::RayTracingPipelineCreateInfoKHR pipeline_info{};
	pipeline_info.stageCount = shader_stages.size();
	pipeline_info.pStages = shader_stages.data();
	pipeline_info.groupCount = shader_groups.size();
	pipeline_info.pGroups = shader_groups.data();
	pipeline_info.maxPipelineRayRecursionDepth = 3;
	pipeline_info.layout = *rt_pipeline_layout;
	pipeline_info.flags = vk::PipelineCreateFlagBits::eDescriptorBufferEXT;

	auto [res, val] = VkContext::GetLogicalDevice().device->createRayTracingPipelineKHRUnique(nullptr, nullptr, pipeline_info);
	SNK_CHECK_VK_RESULT(res);
	rt_pipeline = std::move(val);

	sbt_group_count = shader_groups.size();
}

void RT::CreateShaderBindingTable() {
	auto& ray_pipeline_properties = VkContext::GetPhysicalDevice().ray_properties;
	auto sbt_handle_size = ray_pipeline_properties.shaderGroupHandleSize;
	auto sbt_handle_alignment = ray_pipeline_properties.shaderGroupHandleAlignment;
	sbt_handle_size_aligned = aligned_size(sbt_handle_size, sbt_handle_alignment);
	auto sbt_size = sbt_group_count * sbt_handle_size_aligned;

	std::vector<std::byte> sbt_data(sbt_size);
	auto res = VkContext::GetLogicalDevice().device->getRayTracingShaderGroupHandlesKHR(*rt_pipeline, 0u, sbt_group_count, (size_t)sbt_size, sbt_data.data());
	SNK_CHECK_VK_RESULT(res);

	std::vector<S_VkBuffer*> sbt_bufs = { &sbt_ray_gen_buffer, &sbt_ray_miss_buffer, &sbt_ray_hit_buffer };
	for (size_t i = 0; i < sbt_bufs.size(); i++) {
		auto* p_buf = sbt_bufs[i];
		p_buf->CreateBuffer(sbt_handle_size, vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

		memcpy(p_buf->Map(), sbt_data.data() + i * sbt_handle_size_aligned, sbt_handle_size);
	}

}

void RT::RecordRenderCmdBuf(vk::CommandBuffer cmd, Image2D& output_image, DescriptorBuffer& common_ubo_db) {
	vk::StridedDeviceAddressRegionKHR raygen_sbt{};
	raygen_sbt.deviceAddress = sbt_ray_gen_buffer.GetDeviceAddress();
	raygen_sbt.stride = sbt_handle_size_aligned;
	raygen_sbt.size = sbt_handle_size_aligned;

	vk::StridedDeviceAddressRegionKHR raymiss_sbt{};
	raymiss_sbt.deviceAddress = sbt_ray_miss_buffer.GetDeviceAddress();
	raymiss_sbt.stride = sbt_handle_size_aligned;
	raymiss_sbt.size = sbt_handle_size_aligned;

	vk::StridedDeviceAddressRegionKHR ray_hit_sbt{};
	ray_hit_sbt.deviceAddress = sbt_ray_hit_buffer.GetDeviceAddress();
	ray_hit_sbt.stride = sbt_handle_size_aligned;
	ray_hit_sbt.size = sbt_handle_size_aligned;

	vk::StridedDeviceAddressRegionKHR ray_callable_sbt{};

	vk::CommandBufferBeginInfo begin_info{};

	SNK_CHECK_VK_RESULT(cmd.begin(begin_info));

	output_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
		vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderWrite, vk::PipelineStageFlagBits::eRayTracingShaderKHR,
		vk::PipelineStageFlagBits::eRayTracingShaderKHR, 0, 1, cmd);

	cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *rt_pipeline);
	std::array<uint32_t, 2> db_indices = { 0, 1 };
	std::array<vk::DeviceSize, 2> db_offsets = { 0, 0 };

	auto binding_infos = util::array(common_ubo_db.GetBindingInfo(), rt_descriptor_buffers[VkContext::GetCurrentFIF()].GetBindingInfo());

	cmd.bindDescriptorBuffersEXT(binding_infos);
	cmd.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eRayTracingKHR, *rt_pipeline_layout, 0, db_indices, db_offsets);

	auto& spec = output_image.GetSpec();

	cmd.traceRaysKHR(raygen_sbt, raymiss_sbt, ray_hit_sbt, ray_callable_sbt, spec.size.x, spec.size.y, 1);

	SNK_CHECK_VK_RESULT(cmd.end());
}

void RT::Init(Scene& scene, Image2D& output_image, vk::DescriptorSetLayout common_ubo_set_layout) {
	frame_start_listener.callback = [&](Event const* p_event) {
		//InitTLAS(scene, VkContext::GetCurrentFIF());
		};
	EventManagerG::RegisterListener<FrameStartEvent>(frame_start_listener);

	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		InitTLAS(scene, i);
	}

	InitDescriptorBuffers(output_image, scene);
	InitPipeline(common_ubo_set_layout);
	CreateShaderBindingTable();
}

void BLAS::GenerateFromMeshData(MeshData& mesh_data) {
	auto& logical_device = VkContext::GetLogicalDevice();
	vk::AccelerationStructureGeometryKHR as_geom_info{};
	as_geom_info.flags = vk::GeometryFlagBitsKHR::eOpaque;
	as_geom_info.geometryType = vk::GeometryTypeKHR::eTriangles;

	S_VkBuffer position_buf{};
	S_VkBuffer index_buf{};

	position_buf.CreateBuffer(mesh_data.num_vertices * sizeof(aiVector3D), vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
		VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

	index_buf.CreateBuffer(mesh_data.num_indices * sizeof(unsigned), vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
		VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

	memcpy(position_buf.Map(), mesh_data.positions, mesh_data.num_vertices * sizeof(aiVector3D));
	memcpy(index_buf.Map(), mesh_data.indices, mesh_data.num_indices * sizeof(unsigned));

	as_geom_info.geometry.triangles.vertexData = position_buf.GetDeviceAddress();
	as_geom_info.geometry.triangles.indexData = index_buf.GetDeviceAddress();
	as_geom_info.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
	as_geom_info.geometry.triangles.maxVertex = mesh_data.num_vertices;
	as_geom_info.geometry.triangles.vertexStride = sizeof(glm::vec3);
	as_geom_info.geometry.triangles.indexType = vk::IndexType::eUint32;

	vk::AccelerationStructureBuildGeometryInfoKHR as_build_geom_info{};
	as_build_geom_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
	as_build_geom_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
	as_build_geom_info.geometryCount = 1;
	as_build_geom_info.pGeometries = &as_geom_info;

	vk::AccelerationStructureBuildSizesInfoKHR as_build_sizes_info = logical_device.device->getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice, as_build_geom_info, mesh_data.num_indices / 3);

	m_blas_buf.CreateBuffer(as_build_sizes_info.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
		vk::BufferUsageFlagBits::eShaderDeviceAddress);

	vk::AccelerationStructureCreateInfoKHR as_info{};
	as_info.buffer = m_blas_buf.buffer;
	as_info.size = as_build_sizes_info.accelerationStructureSize;
	as_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;

	auto [res, val] = logical_device.device->createAccelerationStructureKHRUnique(as_info);
	SNK_CHECK_VK_RESULT(res);
	mp_blas = std::move(val);

	S_VkBuffer blas_scratch_buf;
	blas_scratch_buf.CreateBuffer(as_build_sizes_info.buildScratchSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);

	vk::AccelerationStructureBuildGeometryInfoKHR as_build_geom_info_scratch{};
	as_build_geom_info_scratch.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
	as_build_geom_info_scratch.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
	as_build_geom_info_scratch.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
	as_build_geom_info_scratch.dstAccelerationStructure = *mp_blas;
	as_build_geom_info_scratch.geometryCount = 1;
	as_build_geom_info_scratch.pGeometries = &as_geom_info;
	as_build_geom_info_scratch.scratchData.deviceAddress = blas_scratch_buf.GetDeviceAddress();

	vk::AccelerationStructureBuildRangeInfoKHR as_build_range_info{};
	as_build_range_info.primitiveCount = mesh_data.num_indices / 3;
	as_build_range_info.primitiveOffset = 0;
	as_build_range_info.firstVertex = 0;
	as_build_range_info.transformOffset = 0;
	std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> build_range_infos = { &as_build_range_info };

	auto cmd = BeginSingleTimeCommands();
	cmd->buildAccelerationStructuresKHR(as_build_geom_info_scratch, build_range_infos);
	EndSingleTimeCommands(*cmd);

	vk::AccelerationStructureDeviceAddressInfoKHR as_address_info{};
	as_address_info.accelerationStructure = *mp_blas;
	m_blas_handle = logical_device.device->getAccelerationStructureAddressKHR(as_address_info);

	SNK_ASSERT(mp_blas);
}

vk::AccelerationStructureInstanceKHR BLAS::GenerateInstance(TransformComponent& comp) {
	vk::AccelerationStructureInstanceKHR instance{};
	instance.mask = 0xFF;
	instance.instanceShaderBindingTableRecordOffset = 0;
	instance.flags = 0;
	instance.instanceCustomIndex = comp.GetEntity()->GetComponent<TransformBufferIdxComponent>()->idx;
	instance.accelerationStructureReference = m_blas_handle; // Handle of BLAS for the sphere mesh

	auto t = transpose(comp.GetMatrix());
	memcpy(instance.transform.matrix.data(), &t, sizeof(float) * 3 * 4);

	return instance;
}