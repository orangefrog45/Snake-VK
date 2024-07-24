#include "pch/pch.h"
#include "core/VkCommon.h"
#include "core/VkContext.h"
#include "util/util.h"

namespace SNAKE {
	vk::UniqueCommandBuffer BeginSingleTimeCommands(vk::CommandPool pool) {
		vk::CommandBufferAllocateInfo alloc_info{};
		alloc_info.level = vk::CommandBufferLevel::ePrimary;
		alloc_info.commandPool = pool;
		alloc_info.commandBufferCount = 1;

		// Create new command buffer to perform memory transfer
		auto [result, bufs] = VulkanContext::GetLogicalDevice().device->allocateCommandBuffersUnique(alloc_info);
		SNK_CHECK_VK_RESULT(
			result
		);

		vk::UniqueCommandBuffer cmd_buf = std::move(bufs[0]);

		vk::CommandBufferBeginInfo begin_info{};
		begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

		// Start recording
		SNK_CHECK_VK_RESULT(
			cmd_buf->begin(begin_info)
		);

		return std::move(cmd_buf);
	}

	void EndSingleTimeCommands(vk::CommandBuffer& cmd_buf) {
		// Stop recording
		SNK_CHECK_VK_RESULT(
			cmd_buf.end()
		);

		vk::SubmitInfo submit_info{};
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmd_buf;

		auto& device = VulkanContext::GetLogicalDevice();

		SNK_CHECK_VK_RESULT(
			device.graphics_queue.submit(submit_info)
		);

		// Wait for queue to become idle
		SNK_CHECK_VK_RESULT(device.graphics_queue.waitIdle());
	}

	void CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height, vk::CommandPool cmd_pool) {
		auto cmd_buf = BeginSingleTimeCommands(cmd_pool);

		vk::BufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0; // pixels tightly packed
		region.bufferImageHeight = 0; // pixels tightly packed
		region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = vk::Offset3D{ 0, 0, 0 };
		region.imageExtent = vk::Extent3D{ width, height, 1 };

		cmd_buf->copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

		EndSingleTimeCommands(*cmd_buf);
	}

	uint32_t FindMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties) {
		auto phys_mem_properties = VulkanContext::GetPhysicalDevice().device.getMemoryProperties();

		for (uint32_t i = 0; i < phys_mem_properties.memoryTypeCount; i++) {
			if (type_filter & (1 << i) &&
				(static_cast<uint32_t>(phys_mem_properties.memoryTypes[i].propertyFlags) & static_cast<uint32_t>(properties)) == static_cast<uint32_t>(properties)) {
				return i;
			}
		}

		SNK_BREAK("Failed to find memory type");
		return 0;
	}

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

	SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
		SwapChainSupportDetails details;
		SNK_CHECK_VK_RESULT(
			device.getSurfaceCapabilitiesKHR(surface, &details.capabilities, VULKAN_HPP_DEFAULT_DISPATCHER)
		);

		uint32_t num_formats;
		SNK_CHECK_VK_RESULT(
			device.getSurfaceFormatsKHR(surface, &num_formats, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER)
		);

		if (num_formats != 0) {
			details.formats.resize(num_formats);
			SNK_CHECK_VK_RESULT(
				device.getSurfaceFormatsKHR(surface, &num_formats, details.formats.data(), VULKAN_HPP_DEFAULT_DISPATCHER)
			);
		}

		uint32_t present_mode_count;
		SNK_CHECK_VK_RESULT(
			device.getSurfacePresentModesKHR(surface, &present_mode_count, nullptr)
		);

		if (present_mode_count != 0) {
			details.present_modes.resize(present_mode_count);
			SNK_CHECK_VK_RESULT(
				device.getSurfacePresentModesKHR(surface, &present_mode_count, details.present_modes.data())
			);
		}

		return details;
	};


	QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface) {
		uint32_t qf_count = 0;

		device.getQueueFamilyProperties(&qf_count, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER);

		std::vector<vk::QueueFamilyProperties> queue_families(qf_count);
		device.getQueueFamilyProperties(&qf_count, queue_families.data(), VULKAN_HPP_DEFAULT_DISPATCHER);

		for (uint32_t i = 0; i < queue_families.size(); i++) {
			QueueFamilyIndices indices;
			if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics)
				indices.graphics_family = i;

			auto [result, present_support] = device.getSurfaceSupportKHR(i, surface, VULKAN_HPP_DEFAULT_DISPATCHER);
			if (present_support)
				indices.present_family = i;

			if (indices.IsComplete())
				return indices;
		}

		return QueueFamilyIndices{};
	}

	bool CheckDeviceExtensionSupport(vk::PhysicalDevice device, const std::vector<const char*>& required_extensions_vec) {
		uint32_t extension_count;

		SNK_DBG_CHECK_VK_RESULT(
			device.enumerateDeviceExtensionProperties(nullptr, &extension_count, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER),
			"Physical device extension enumeration"
		);

		std::vector<vk::ExtensionProperties> available_extensions(extension_count);

		SNK_DBG_CHECK_VK_RESULT(
			device.enumerateDeviceExtensionProperties(nullptr, &extension_count, available_extensions.data(), VULKAN_HPP_DEFAULT_DISPATCHER),
			"Physical device extension enumeration"
		);

		std::set<std::string> required_extensions(required_extensions_vec.begin(), required_extensions_vec.end());

		for (const auto& extension : available_extensions) {
			required_extensions.erase(extension.extensionName);
		}

		return required_extensions.empty();
	}
}
