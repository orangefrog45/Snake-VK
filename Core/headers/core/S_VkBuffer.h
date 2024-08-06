#pragma once
#include <vk_mem_alloc.h>
#include "core/VkCommon.h"
#include "core/VkIncl.h"

namespace SNAKE {
	class S_VkBuffer {
	public:
		S_VkBuffer() = default;
		S_VkBuffer(const S_VkBuffer& other) = delete;
		S_VkBuffer& operator=(const S_VkBuffer& other) = delete;

		S_VkBuffer(S_VkBuffer&& other) noexcept : 
			allocation(std::move(other.allocation)), 
			buffer(std::move(other.buffer)) {};

		~S_VkBuffer() {
			vmaDestroyBuffer(VulkanContext::GetAllocator(), buffer, allocation);
		}

		void CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags = 0);

		std::pair<vk::DescriptorGetInfoEXT, std::shared_ptr<vk::DescriptorAddressInfoEXT>> CreateDescriptorGetInfo();

		vk::DeviceAddress GetDeviceAddress() {
			vk::BufferDeviceAddressInfo buffer_addr_info{};
			buffer_addr_info.buffer = buffer;
			return VulkanContext::GetLogicalDevice().device->getBufferAddress(buffer_addr_info);
		}

		inline void* Map() {
			if (p_data) return p_data;

			SNK_CHECK_VK_RESULT(vmaMapMemory(VulkanContext::GetAllocator(), allocation, &p_data));
			return p_data;
		}

		inline void Unmap() {
			vmaUnmapMemory(VulkanContext::GetAllocator(), allocation);
			p_data = nullptr;
		}

		VmaAllocation allocation;
		VmaAllocationInfo alloc_info;
		vk::Buffer buffer;

	private:
		vk::BufferUsageFlags m_usage;
		// Mapped ptr, nullptr if not mapped
		void* p_data = nullptr;
	};
}
