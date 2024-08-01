#pragma once
#include "core/VkIncl.h"
#include "core/VkContext.h"
#include <vk_mem_alloc.h>
#include <optional>

namespace SNAKE {
	struct QueueFamilyIndices {
		std::optional<uint32_t> graphics_family;
		std::optional<uint32_t> present_family;

		bool IsComplete() {
			return graphics_family.has_value() && present_family.has_value();
		}
	};

	struct SwapChainSupportDetails {
		vk::SurfaceCapabilitiesKHR capabilities;
		std::vector<vk::SurfaceFormatKHR> formats;
		std::vector<vk::PresentModeKHR> present_modes;
	};

	struct S_VkBuffer {
		S_VkBuffer() = default;
		S_VkBuffer(const S_VkBuffer& other) = delete;
		S_VkBuffer& operator=(const S_VkBuffer& other) = delete;

		S_VkBuffer& operator=(S_VkBuffer&& other) noexcept {
			this->allocation = std::move(other.allocation);
			this->buffer = std::move(other.buffer);

			return *this;
		}

		~S_VkBuffer() {
			vmaDestroyBuffer(VulkanContext::GetAllocator(), buffer, allocation);
		}

		void CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, VmaAllocationCreateFlags flags = 0);

		vk::DeviceAddress GetDeviceAddress() {
			vk::BufferDeviceAddressInfo buffer_addr_info{};
			buffer_addr_info.buffer = buffer;
			return VulkanContext::GetLogicalDevice().device->getBufferAddress(buffer_addr_info);
		}

		inline void* Map() { 
			void* p_data = nullptr; 
			SNK_CHECK_VK_RESULT(vmaMapMemory(VulkanContext::GetAllocator(), allocation, &p_data)); 
			return p_data;
		}

		inline void Unmap() {
			vmaUnmapMemory(VulkanContext::GetAllocator(), allocation);
		}

		VmaAllocation allocation;
		VmaAllocationInfo alloc_info;
		vk::Buffer buffer;
	};

	SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

	QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);

	bool CheckDeviceExtensionSupport(vk::PhysicalDevice device, const std::vector<const char*>& required_extensions);

	uint32_t FindMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties);

	void CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height, vk::CommandPool pool);

	vk::UniqueCommandBuffer BeginSingleTimeCommands(vk::CommandPool pool);

	void EndSingleTimeCommands(vk::CommandBuffer& cmd_buf);

	void CopyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size, vk::CommandPool pool);
};

