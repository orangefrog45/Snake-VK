#pragma once
#include "core/VkIncl.h"
#include "core/VkContext.h"
#include <vk_mem_alloc.h>
#include <optional>

namespace SNAKE {
	constexpr FrameInFlightIndex MAX_FRAMES_IN_FLIGHT = 2;

	struct QueueFamilyIndices {
		std::optional<uint32_t> graphics_family;
		std::optional<uint32_t> present_family;

		bool IsComplete() {
			return graphics_family.has_value() && present_family.has_value();
		}
	};

	enum class DescriptorSetIndices {
		CAMERA_MATRIX_UBO = 0,
		GLOBAL_TEX_MAT,
		LIGHTS
	};

	struct SwapChainSupportDetails {
		vk::SurfaceCapabilitiesKHR capabilities;
		std::vector<vk::SurfaceFormatKHR> formats;
		std::vector<vk::PresentModeKHR> present_modes;
	};

	struct Vertex {
		// Struct which describes how to move through the data provided
		[[nodiscard]] constexpr inline static std::array<vk::VertexInputBindingDescription, 3> GetBindingDescription() {
			std::array<vk::VertexInputBindingDescription, 3> descs{};

			descs[0].binding = 0;
			descs[0].stride = sizeof(glm::vec3);
			descs[0].inputRate = vk::VertexInputRate::eVertex;

			descs[1].binding = 1;
			descs[1].stride = sizeof(glm::vec3);
			descs[1].inputRate = vk::VertexInputRate::eVertex;

			descs[2].binding = 2;
			descs[2].stride = sizeof(glm::vec2);
			descs[2].inputRate = vk::VertexInputRate::eVertex;

			return descs;
		}

		// Struct which describes vertex attribute access in shaders
		[[nodiscard]] constexpr inline static std::array<vk::VertexInputAttributeDescription, 3> GetAttributeDescriptions() {
			std::array<vk::VertexInputAttributeDescription, 3> descs{};
			// Positions
			descs[0].binding = 0;
			descs[0].location = 0;
			descs[0].format = vk::Format::eR32G32B32Sfloat;
			descs[0].offset = 0;

			// Normals
			descs[1].binding = 1;
			descs[1].location = 1;
			descs[1].format = vk::Format::eR32G32B32Sfloat;
			descs[1].offset = 0;

			// Texture coordinates
			descs[2].binding = 2;
			descs[2].location = 2;
			descs[2].format = vk::Format::eR32G32Sfloat;
			descs[2].offset = 0;

			return descs;
		}
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

	void CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height);

	vk::UniqueCommandBuffer BeginSingleTimeCommands();

	void EndSingleTimeCommands(vk::CommandBuffer& cmd_buf);

	void CopyBuffer(vk::Buffer src, vk::Buffer dst, vk::DeviceSize size);

	inline constexpr vk::DeviceSize aligned_size(vk::DeviceSize value, vk::DeviceSize alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}

	inline vk::Format FindSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
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

	inline bool HasStencilComponent(vk::Format format) {
		return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
	}

	inline vk::Format FindDepthFormat() {
		return FindSupportedFormat({ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
			vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
	}

	inline static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
		[[maybe_unused]] void* p_user_data
	) {
		std::string type_str;
		switch (type) {
		case (int)vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral:
			type_str = "general";
			break;
		case (int)vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance:
			type_str = "performance";
			break;
		case (int)vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation:
			type_str = "validation";
			break;
		}

		switch (severity) {
		case (int)vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
			SNK_CORE_ERROR("Vulkan error '{0}': {1}", type_str, p_callback_data->pMessage);
			SNK_DEBUG_BREAK("Breaking for vulkan error");
			break;
		case (int)vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
			SNK_CORE_INFO("Vulkan info '{0}': {1}", type_str, p_callback_data->pMessage);
			break;
		case (int)vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
			SNK_CORE_WARN("Vulkan warning '{0}': {1}", type_str, p_callback_data->pMessage);
			break;
		case (int)vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
			SNK_CORE_TRACE("Vulkan verbose info '{0}': {1}", type_str, p_callback_data->pMessage);
			break;
		default:
			break;
		}

		return VK_FALSE;
	};

};

