#include "pch/pch.h"
#include "core/VkCommon.h"
#include "resources/TLAS.h"
#include "assets/AssetManager.h"
#include "components/Components.h"
#include "scene/RaytracingBufferSystem.h"

using namespace SNAKE;

void TLAS::BuildFromInstances(const std::vector<vk::AccelerationStructureInstanceKHR>& instances) {
	uint32_t max_primitive_count = 0;

	for (auto* p_mesh : AssetManager::GetView<MeshDataAsset, true>()) {
		for (auto& submesh : p_mesh->submeshes) {
			max_primitive_count = glm::max(submesh.num_indices / 3, max_primitive_count);
		}
	}

	auto& logical_device = VkContext::GetLogicalDevice();

	instance_buffer.CreateBuffer(sizeof(vk::AccelerationStructureInstanceKHR) * glm::max((uint32_t)instances.size(), 1u), vk::BufferUsageFlagBits::eShaderDeviceAddress |
		vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
	auto* p_instance_data = instance_buffer.Map();
	memcpy(p_instance_data, instances.data(), sizeof(vk::AccelerationStructureInstanceKHR) * instances.size());

	vk::AccelerationStructureGeometryKHR as_geom_info{};
	as_geom_info.flags = vk::GeometryFlagBitsKHR::eOpaque;
	as_geom_info.geometryType = vk::GeometryTypeKHR::eInstances;
	as_geom_info.geometry.instances.arrayOfPointers = false;
	as_geom_info.geometry.instances.data = instance_buffer.GetDeviceAddress();
	as_geom_info.geometry.instances.sType = vk::StructureType::eAccelerationStructureGeometryInstancesDataKHR;

	vk::AccelerationStructureBuildGeometryInfoKHR as_build_size_geom_info{};
	as_build_size_geom_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
	as_build_size_geom_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
	as_build_size_geom_info.geometryCount = 1;
	as_build_size_geom_info.pGeometries = &as_geom_info;

	vk::AccelerationStructureBuildSizesInfoKHR as_build_size_info = logical_device.device->getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice, as_build_size_geom_info, max_primitive_count);

	m_as_buffer.CreateBuffer(as_build_size_info.accelerationStructureSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);

	vk::AccelerationStructureCreateInfoKHR as_info{};
	as_info.buffer = m_as_buffer.buffer;
	as_info.size = as_build_size_info.accelerationStructureSize;
	as_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;

	auto [res, value] = logical_device.device->createAccelerationStructureKHRUnique(as_info);
	SNK_CHECK_VK_RESULT(res);
	m_as = std::move(value);

	S_VkBuffer tlas_scratch_buf;
	tlas_scratch_buf.CreateBuffer(as_build_size_info.buildScratchSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);

	vk::AccelerationStructureBuildGeometryInfoKHR as_build_geom_info{};
	as_build_geom_info.type = vk::AccelerationStructureTypeKHR::eTopLevel;
	as_build_geom_info.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
	as_build_geom_info.dstAccelerationStructure = *m_as;
	as_build_geom_info.geometryCount = 1;
	as_build_geom_info.pGeometries = &as_geom_info;
	as_build_geom_info.scratchData.deviceAddress = tlas_scratch_buf.GetDeviceAddress();
	//as_build_geom_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;

	vk::AccelerationStructureBuildRangeInfoKHR as_build_range_info{};
	as_build_range_info.primitiveCount = (uint32_t)instances.size();
	as_build_range_info.primitiveOffset = 0;
	as_build_range_info.firstVertex = 0;
	as_build_range_info.transformOffset = 0;

	auto cmd = BeginSingleTimeCommands();
	cmd->buildAccelerationStructuresKHR(as_build_geom_info, &as_build_range_info);
	EndSingleTimeCommands(*cmd);

	vk::AccelerationStructureDeviceAddressInfoKHR device_address_info{};
	device_address_info.accelerationStructure = *m_as;
	m_tlas_handle = logical_device.device->getAccelerationStructureAddressKHR(device_address_info);
	SNK_ASSERT(m_tlas_handle);

	DispatchResourceEvent(S_VkResourceEvent::ResourceEventType::CREATE);
}

void TLAS::RefreshDescriptorGetInfo(DescriptorGetInfo& info) const {
	std::get<vk::DeviceAddress>(info.resource_info) = m_tlas_handle;
}

void TLAS::Destroy() {
	m_as.reset();
	instance_buffer.DestroyBuffer();
	m_as_buffer.DestroyBuffer();
	DispatchResourceEvent(S_VkResourceEvent::ResourceEventType::DELETE);
}

DescriptorGetInfo TLAS::CreateDescriptorGetInfo() const {
	SNK_ASSERT(m_tlas_handle);
	DescriptorGetInfo ret;
	ret.get_info.type = vk::DescriptorType::eAccelerationStructureKHR;
	ret.resource_info = m_tlas_handle;
	return ret;
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
	blas_scratch_buf.CreateBuffer(as_build_sizes_info.buildScratchSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);

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
	//instance.flags = static_cast<VkGeometryInstanceFlagBitsKHR>(vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable);
	instance.instanceCustomIndex = comp.GetEntity()->GetComponent<RaytracingInstanceBufferIdxComponent>()->idx + m_submesh_index;
	instance.accelerationStructureReference = m_blas_handle; // Handle of BLAS for the sphere mesh

	auto t = glm::transpose(comp.GetMatrix());
	memcpy(instance.transform.matrix.data(), &t, sizeof(float) * 3 * 4);

	return instance;
}