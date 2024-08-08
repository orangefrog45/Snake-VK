#include "core/S_VkBuffer.h"

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
		SNK_CHECK_VK_RESULT(vmaCreateBuffer(VulkanContext::GetAllocator(), &buf_info,
			&alloc_create_info, reinterpret_cast<VkBuffer*>(&buffer), &allocation, &alloc_info));
	}

	std::pair<vk::DescriptorGetInfoEXT, std::shared_ptr<vk::DescriptorAddressInfoEXT>> S_VkBuffer::CreateDescriptorGetInfo() {
		if (!(m_usage & vk::BufferUsageFlagBits::eUniformBuffer))
			SNK_BREAK("Unsupported buffer type used for CreateDescriptorGetInfo");

		auto addr_info = std::make_shared<vk::DescriptorAddressInfoEXT>();
		addr_info->address = GetDeviceAddress(); 
		addr_info->range = alloc_info.size;

		// Use above address + size data to connect the descriptor at the offset provided to this specific UBO
		vk::DescriptorGetInfoEXT buffer_descriptor_info{};
		buffer_descriptor_info.type = vk::DescriptorType::eUniformBuffer;
		buffer_descriptor_info.data.pUniformBuffer = &*addr_info;

		return std::make_pair(buffer_descriptor_info, addr_info);
	}

}