#include "pch/pch.h"
#include "core/VkCommon.h"
#include "resources/TLAS.h"

using namespace SNAKE;

void TLAS::BuildFromInstances(const std::vector<vk::AccelerationStructureInstanceKHR>& instances) {
	auto& logical_device = VkContext::GetLogicalDevice();

	instance_buffer.CreateBuffer(sizeof(vk::AccelerationStructureInstanceKHR) * instances.size(), vk::BufferUsageFlagBits::eShaderDeviceAddress |
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
		vk::AccelerationStructureBuildTypeKHR::eDevice, as_build_size_geom_info, 3800);

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
	as_build_geom_info.flags = vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;

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
	m_as.release();
	instance_buffer.DestroyBuffer();
	m_as_buffer.DestroyBuffer();
	DispatchResourceEvent(S_VkResourceEvent::ResourceEventType::DELETE);
}

DescriptorGetInfo TLAS::CreateDescriptorGetInfo() {
	DescriptorGetInfo ret;
	ret.get_info.type = vk::DescriptorType::eAccelerationStructureKHR;
	ret.resource_info = m_tlas_handle;
	return ret;
}