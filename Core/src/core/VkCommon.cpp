#include "pch/pch.h"
#include "core/VkCommon.h"
#include "core/VkContext.h"
#include "util/util.h"

namespace SNAKE {
	vk::UniqueCommandBuffer BeginSingleTimeCommands() {
		vk::CommandBufferAllocateInfo alloc_info{};
		alloc_info.level = vk::CommandBufferLevel::ePrimary;
		alloc_info.commandPool = VulkanContext::GetCommandPool();
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

	void EndSingleTimeCommands(vk::CommandBuffer& cmd_buf, std::optional<std::pair<vk::Semaphore, vk::PipelineStageFlags>> wait_semaphore_stage) {
		// Stop recording
		SNK_CHECK_VK_RESULT(
			cmd_buf.end()
		);

		vk::SubmitInfo submit_info{};
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmd_buf;
		if (wait_semaphore_stage.has_value()) {
			submit_info.pWaitSemaphores = &wait_semaphore_stage.value().first;
			submit_info.waitSemaphoreCount = 1;
			submit_info.pWaitDstStageMask = &wait_semaphore_stage.value().second;
		}

		auto& device = VulkanContext::GetLogicalDevice();

		SNK_CHECK_VK_RESULT(
			device.graphics_queue.submit(submit_info)
		);

		// Wait for queue to become idle
		SNK_CHECK_VK_RESULT(device.graphics_queue.waitIdle());
	}

	void CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height) {
		auto cmd_buf = BeginSingleTimeCommands();

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

	void CopyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) {
		vk::UniqueCommandBuffer cmd_buf = BeginSingleTimeCommands();

		vk::BufferCopy copy_region{};
		copy_region.srcOffset = 0;
		copy_region.dstOffset = 0;
		copy_region.size = size;

		cmd_buf->copyBuffer(src, dst, copy_region);

		EndSingleTimeCommands(*cmd_buf);
	}

	vk::Format FindSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
		for (auto format : candidates) {
			vk::FormatProperties properties = VulkanContext::GetPhysicalDevice().device.getFormatProperties(format);
			if (tiling == vk::ImageTiling::eLinear && (properties.linearTilingFeatures & features) == features) {
				return format;
			}
			else if (tiling == vk::ImageTiling::eOptimal && (properties.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		SNK_BREAK("No supported format found");
		return vk::Format::eUndefined;
	}
}
