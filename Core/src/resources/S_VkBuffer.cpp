#include "resources/S_VkBuffer.h"

namespace SNAKE {
	void S_VkBuffer::CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags) {
		m_usage = usage;

		vk::BufferCreateInfo buffer_info{};
		buffer_info.size = size;
		buffer_info.usage = usage;

		VmaAllocationCreateInfo alloc_create_info{};
		alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;
		alloc_create_info.flags = flags;

		auto buf_info = static_cast<VkBufferCreateInfo>(buffer_info);
		SNK_CHECK_VK_RESULT(vmaCreateBuffer(VkContext::GetAllocator(), &buf_info,
			&alloc_create_info, reinterpret_cast<VkBuffer*>(&buffer), &allocation, &alloc_info));

		DispatchResourceEvent(S_VkResourceEvent::ResourceEventType::CREATE);
	}

	void S_VkBuffer::RefreshDescriptorGetInfo(DescriptorGetInfo& info) const {
		auto& address_info = std::get<vk::DescriptorAddressInfoEXT>(info.resource_info);
		address_info.address = GetDeviceAddress();
		address_info.range = alloc_info.size;
	}

	DescriptorGetInfo S_VkBuffer::CreateDescriptorGetInfo() const {
		DescriptorGetInfo ret;

		if (m_usage & vk::BufferUsageFlagBits::eStorageBuffer)
			ret.get_info.type = vk::DescriptorType::eStorageBuffer;
		else if (m_usage & vk::BufferUsageFlagBits::eUniformBuffer)
			ret.get_info.type = vk::DescriptorType::eUniformBuffer;
		else 
			SNK_BREAK("Unsupported buffer type used for CreateDescriptorGetInfo");

		ret.resource_info = vk::DescriptorAddressInfoEXT(GetDeviceAddress(), alloc_info.size);

		return ret;
	}

}