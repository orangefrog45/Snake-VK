#include "pch/pch.h"
#include "rendering/Raytracing.h"
#include "scene/RaytracingBufferSystem.h"
#include "components/Components.h"
#include "scene/LightBufferSystem.h"
#include "rendering/RenderCommon.h"

using namespace SNAKE;

void RT::InitTLAS(Scene& scene, FrameInFlightIndex idx) {

	if (tlas_array[idx].GetAS()) {
		tlas_array[idx].Destroy();
	}

	std::vector<vk::AccelerationStructureInstanceKHR> instances;

	auto& reg = scene.GetRegistry();
	auto mesh_view = reg.view<StaticMeshComponent>();
	if (mesh_view.empty())
		return;

	for (auto [entity, mesh] : mesh_view.each()) {
		for (auto& blas : mesh.GetMeshAsset()->data->submesh_blas_array) {
			instances.push_back(blas->GenerateInstance(reg.get<TransformComponent>(entity)));
		}
	}

	tlas_array[idx].BuildFromInstances(instances);
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

void RT::InitDescriptorBuffers(Image2D& output_image, Scene& scene, GBufferResources& output_gbuffer) {
	rt_descriptor_set_spec = std::make_shared<DescriptorSetSpec>();
	rt_descriptor_set_spec->AddDescriptor(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eAll) // TLAS
		.AddDescriptor(1, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR) // Output image
		.AddDescriptor(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR) // Vertex position buffer
		.AddDescriptor(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR) // Vertex index buffer
		.AddDescriptor(4, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR) // Vertex normal buffer
		.AddDescriptor(5, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR) // Vertex tex coord buffer
		.AddDescriptor(6, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR) // Vertex tangent buffer
		.AddDescriptor(7, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR) // Raytracing instance buffer
		.AddDescriptor(8, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR)  // Light buffer
		.AddDescriptor(9, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR) // Input gbuffer albedo combined image sampler
		.AddDescriptor(10, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR) // Input gbuffer normal combined image sampler
		.AddDescriptor(11, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR) // Input gbuffer depth combined image sampler
		.AddDescriptor(12, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR) // Input gbuffer rma combined image sampler
		.GenDescriptorLayout();

	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		rt_descriptor_buffers[i].SetDescriptorSpec(rt_descriptor_set_spec);
		rt_descriptor_buffers[i].CreateBuffer(1);
		
		auto mesh_buffers = AssetManager::Get().mesh_buffer_manager.GetMeshBuffers();

		auto get_info_tlas = tlas_array[i].CreateDescriptorGetInfo();
		auto get_info_image = output_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage);
		auto get_info_position_buf = mesh_buffers.position_buf.CreateDescriptorGetInfo();
		auto get_info_index_buf = mesh_buffers.indices_buf.CreateDescriptorGetInfo();
		auto get_info_normal_buf = mesh_buffers.normal_buf.CreateDescriptorGetInfo();
		auto get_info_tex_coord_buf = mesh_buffers.tex_coord_buf.CreateDescriptorGetInfo();
		auto get_info_tangent_buf = mesh_buffers.tangent_buf.CreateDescriptorGetInfo();
		auto get_info_instance_buf = scene.GetSystem<RaytracingInstanceBufferSystem>()->GetStorageBuffer(i).CreateDescriptorGetInfo();
		auto get_info_light_buf = scene.GetSystem<LightBufferSystem>()->light_ssbos[i].CreateDescriptorGetInfo();
		auto get_info_albedo_gbuffer_image = output_gbuffer.albedo_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler);
		auto get_info_normal_gbuffer_image = output_gbuffer.normal_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler);
		auto get_info_depth_gbuffer_image = output_gbuffer.depth_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler);
		auto get_info_rma_gbuffer_image = output_gbuffer.rma_image.CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eCombinedImageSampler);

		rt_descriptor_buffers[i].LinkResource(&tlas_array[i], get_info_tlas, 0, 0);
		rt_descriptor_buffers[i].LinkResource(&output_image, get_info_image, 1, 0);
		rt_descriptor_buffers[i].LinkResource(&mesh_buffers.position_buf, get_info_position_buf, 2, 0);
		rt_descriptor_buffers[i].LinkResource(&mesh_buffers.indices_buf, get_info_index_buf, 3, 0);
		rt_descriptor_buffers[i].LinkResource(&mesh_buffers.normal_buf, get_info_normal_buf, 4, 0);
		rt_descriptor_buffers[i].LinkResource(&mesh_buffers.tex_coord_buf, get_info_tex_coord_buf, 5, 0);
		rt_descriptor_buffers[i].LinkResource(&mesh_buffers.tangent_buf, get_info_tangent_buf, 6, 0);
		rt_descriptor_buffers[i].LinkResource(&scene.GetSystem<RaytracingInstanceBufferSystem>()->GetStorageBuffer(i), get_info_instance_buf, 7, 0);
		rt_descriptor_buffers[i].LinkResource(&scene.GetSystem<LightBufferSystem>()->light_ssbos[i], get_info_light_buf, 8, 0);
		rt_descriptor_buffers[i].LinkResource(&output_gbuffer.albedo_image, get_info_albedo_gbuffer_image, 9, 0);
		rt_descriptor_buffers[i].LinkResource(&output_gbuffer.normal_image, get_info_normal_gbuffer_image, 10, 0);
		rt_descriptor_buffers[i].LinkResource(&output_gbuffer.depth_image, get_info_depth_gbuffer_image, 11, 0);
		rt_descriptor_buffers[i].LinkResource(&output_gbuffer.rma_image, get_info_rma_gbuffer_image, 12, 0);
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
		.Build();

	pipeline.Init(pipeline_builder, layout_builder);
}

void SBT::Init(vk::Pipeline pipeline, const RtPipelineBuilder& builder) {
	auto& ray_pipeline_properties = VkContext::GetPhysicalDevice().ray_properties;
	auto sbt_handle_size = ray_pipeline_properties.shaderGroupHandleSize;
	auto sbt_handle_alignment = ray_pipeline_properties.shaderGroupHandleAlignment;
	m_sbt_aligned_handle_size = (uint32_t)aligned_size(sbt_handle_size, sbt_handle_alignment);

	// TODO: Base these counts off of shaders added in builder
	uint32_t miss_count = 2;
	uint32_t hit_count = 1;

	address_regions.rgen.stride = aligned_size(m_sbt_aligned_handle_size, ray_pipeline_properties.shaderGroupBaseAlignment);
	address_regions.rgen.size = address_regions.rgen.stride; // For rgen, size must be same as stride

	address_regions.rmiss.stride = m_sbt_aligned_handle_size;
	address_regions.rmiss.size = aligned_size(miss_count * m_sbt_aligned_handle_size, ray_pipeline_properties.shaderGroupBaseAlignment);

	address_regions.rhit.stride = m_sbt_aligned_handle_size;
	address_regions.rhit.size = aligned_size(hit_count * m_sbt_aligned_handle_size, ray_pipeline_properties.shaderGroupBaseAlignment);

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
	for (uint32_t i = 0; i < miss_count; i++) {
		memcpy(p_data, GetHandle(handle_idx++), sbt_handle_size);
		p_data += address_regions.rmiss.stride;
	}

	// Hit
	p_data = p_sbt_buf + address_regions.rgen.size + address_regions.rmiss.size;
	for (uint32_t i = 0; i < hit_count; i++) {
		memcpy(p_data, GetHandle(handle_idx++), sbt_handle_size);
		p_data += address_regions.rhit.stride;
	}
	sbt_buffer.Unmap();

	address_regions.rgen.deviceAddress = sbt_buffer.GetDeviceAddress();
	address_regions.rmiss.deviceAddress = sbt_buffer.GetDeviceAddress() + address_regions.rgen.size;
	address_regions.rhit.deviceAddress = sbt_buffer.GetDeviceAddress() + address_regions.rgen.size + address_regions.rmiss.size;
}

//void RT::RecordGBufferPassCmdBuf(vk::CommandBuffer cmd, GBufferResources& output) {
//	vk::CommandBufferBeginInfo begin_info{};
//	SNK_CHECK_VK_RESULT(cmd.begin(begin_info));
//
//
//	SNK_CHECK_VK_RESULT(cmd.end());
//}

void RT::RecordRenderCmdBuf(vk::CommandBuffer cmd, Image2D& output_image, DescriptorBuffer& common_ubo_db) {

	output_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
		vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderWrite, vk::PipelineStageFlagBits::eRayTracingShaderKHR,
		vk::PipelineStageFlagBits::eRayTracingShaderKHR, 0, 1, cmd);

	cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, pipeline.GetPipeline());
	std::array<uint32_t, 3> db_indices = { 0, 1, 2 };
	std::array<vk::DeviceSize, 3> db_offsets = { 0, 0, 0 };

	auto binding_infos = util::array(common_ubo_db.GetBindingInfo(), rt_descriptor_buffers[VkContext::GetCurrentFIF()].GetBindingInfo(), 
		AssetManager::GetGlobalTexMatBufBindingInfo(VkContext::GetCurrentFIF()));

	cmd.bindDescriptorBuffersEXT(binding_infos);
	cmd.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eRayTracingKHR, pipeline.GetPipelineLayout(), 0, db_indices, db_offsets);

	auto& spec = output_image.GetSpec();

	auto& sbt = pipeline.GetSBT();
	cmd.traceRaysKHR(sbt.address_regions.rgen, sbt.address_regions.rmiss, sbt.address_regions.rhit, sbt.address_regions.callable, spec.size.x, spec.size.y, 1);
}

void RT::Init(Scene& scene, Image2D& output_image, std::weak_ptr<const DescriptorSetSpec> common_ubo_set, GBufferResources& gbuffer) {
	frame_start_listener.callback = [&]([[maybe_unused]] Event const* p_event) {
		InitTLAS(scene, VkContext::GetCurrentFIF());
		};
	EventManagerG::RegisterListener<FrameStartEvent>(frame_start_listener);

	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		InitTLAS(scene, i);
	}

	InitDescriptorBuffers(output_image, scene, gbuffer);
	InitPipeline(common_ubo_set);
}

void BLAS::GenerateFromMeshData(MeshData& mesh_data, uint32_t submesh_index) {
	m_submesh_index = submesh_index;

	auto& logical_device = VkContext::GetLogicalDevice();
	vk::AccelerationStructureGeometryKHR as_geom_info{};
	as_geom_info.flags = vk::GeometryFlagBitsKHR::eOpaque;
	as_geom_info.geometryType = vk::GeometryTypeKHR::eTriangles;

	S_VkBuffer position_buf{};
	S_VkBuffer index_buf{};

	auto& submesh = mesh_data.submeshes[submesh_index];
	position_buf.CreateBuffer(submesh.num_vertices * sizeof(aiVector3D), vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
		VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

	index_buf.CreateBuffer(submesh.num_indices * sizeof(unsigned), vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
		VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

	memcpy(position_buf.Map(), mesh_data.positions + submesh.base_vertex, submesh.num_vertices * sizeof(aiVector3D));
	memcpy(index_buf.Map(), mesh_data.indices + submesh.base_index, submesh.num_indices * sizeof(unsigned));

	as_geom_info.geometry.triangles.vertexData = position_buf.GetDeviceAddress();
	as_geom_info.geometry.triangles.indexData = index_buf.GetDeviceAddress();
	as_geom_info.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
	as_geom_info.geometry.triangles.maxVertex = submesh.num_vertices;
	as_geom_info.geometry.triangles.vertexStride = sizeof(glm::vec3);
	as_geom_info.geometry.triangles.indexType = vk::IndexType::eUint32;

	vk::AccelerationStructureBuildGeometryInfoKHR as_build_geom_info{};
	as_build_geom_info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
	as_build_geom_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
	as_build_geom_info.geometryCount = 1;
	as_build_geom_info.pGeometries = &as_geom_info;

	vk::AccelerationStructureBuildSizesInfoKHR as_build_sizes_info = logical_device.device->getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice, as_build_geom_info, submesh.num_indices);

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
	as_build_range_info.primitiveCount = submesh.num_indices / 3;
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
	instance.flags = static_cast<VkGeometryInstanceFlagBitsKHR>(vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable);
	instance.instanceCustomIndex = comp.GetEntity()->GetComponent<RaytracingInstanceBufferIdxComponent>()->idx + m_submesh_index;
	instance.accelerationStructureReference = m_blas_handle; // Handle of BLAS for the sphere mesh

	auto t = glm::transpose(comp.GetMatrix());
	memcpy(instance.transform.matrix.data(), &t, sizeof(float) * 3 * 4);

	return instance;
}