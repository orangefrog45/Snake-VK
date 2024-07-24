#pragma once
#include <core/VkIncl.h>

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

	SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

	QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);

	bool CheckDeviceExtensionSupport(vk::PhysicalDevice device, const std::vector<const char*>& required_extensions);

	void CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::UniqueBuffer& buffer, vk::UniqueDeviceMemory& buffer_memory);

	uint32_t FindMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties);

	void CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height, vk::CommandPool pool);

	vk::UniqueCommandBuffer BeginSingleTimeCommands(vk::CommandPool pool);

	void EndSingleTimeCommands(vk::CommandBuffer& cmd_buf);
};

