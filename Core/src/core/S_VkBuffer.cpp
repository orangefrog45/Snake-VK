#include "core/S_VkBuffer.h"

namespace SNAKE {
	void S_VkBuffer::CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags) {
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
}