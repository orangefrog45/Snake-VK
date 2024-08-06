#pragma once
#include "core/VkIncl.h"
#include "core/VkContext.h"
#include <vk_mem_alloc.h>
#include <optional>

namespace SNAKE {
	using FrameInFlightIndex = uint8_t;
	constexpr FrameInFlightIndex MAX_FRAMES_IN_FLIGHT = 2;

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

	template<typename T>
	struct VkDescriptorGetInfoPackage {
		T info;
		vk::DescriptorGetInfoEXT descriptor_info;
	};

	SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

	QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);

	bool CheckDeviceExtensionSupport(vk::PhysicalDevice device, const std::vector<const char*>& required_extensions);

	uint32_t FindMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties);

	void CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height, vk::CommandPool pool);

	vk::UniqueCommandBuffer BeginSingleTimeCommands(vk::CommandPool pool);

	void EndSingleTimeCommands(vk::CommandBuffer& cmd_buf);

	void CopyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size, vk::CommandPool pool);

	inline vk::DeviceSize aligned_size(vk::DeviceSize value, vk::DeviceSize alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}
};

