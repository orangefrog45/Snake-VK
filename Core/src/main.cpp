#include <iostream>
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_ASSERT_ON_RESULT
#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "util.h"
#include "FileUtil.h"
#include <filesystem>
#include <glm/glm.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace SNAKE;

constexpr unsigned MAX_FRAMES_IN_FLIGHT = 2;


static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(
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
	}

	return VK_FALSE;
};

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

struct Vertex {
	Vertex(glm::vec2 _pos, glm::vec3 _col) : pos(_pos), colour(_col) {};
	glm::vec2 pos;
	glm::vec3 colour;

	// Struct which describes how to move through the data provided
	[[nodiscard]] constexpr inline static vk::VertexInputBindingDescription GetBindingDescription() {
		vk::VertexInputBindingDescription desc{};
		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = vk::VertexInputRate::eVertex;

		return desc;
	}

	// Struct which describes vertex attribute access in shaders
	[[nodiscard]] constexpr inline static std::array<vk::VertexInputAttributeDescription, 2> GetAttributeDescriptions() {
		std::array<vk::VertexInputAttributeDescription, 2> descs{};

		descs[0].binding = 0;
		descs[0].location = 0;
		descs[0].format = vk::Format::eR32G32Sfloat;
		descs[0].offset = offsetof(Vertex, pos);

		descs[1].binding = 0;
		descs[1].location = 1;
		descs[1].format = vk::Format::eR32G32B32Sfloat;
		descs[1].offset = offsetof(Vertex, colour);

		return descs;
	}
};

struct UBO {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};


class VulkanApp {
public:
	void Init(const char* app_name) {
		InitWindow();

		VULKAN_HPP_DEFAULT_DISPATCHER.init();
		CreateInstance(app_name); 
		VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance.get());

		CreateDebugCallback();
		CreateWindowSurface();
		PickPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain();
		CreateImageViews();
		CreateGraphicsPipeline();
		CreateCommandPool();
		CreateVertexBuffer();
		CreateIndexBuffer();
		CreateCommandBuffers();
		CreateSyncObjects();
	}


	void InitWindow() {
		if (!glfwInit() || !glfwVulkanSupported())
			SNK_BREAK("GLFW failed to initialize");

		// Default API is OpenGL so set to GLFW_NO_API
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, true);

		p_window = glfwCreateWindow(1920, 1080, "ORNG VK", nullptr, nullptr);
		glfwSetWindowUserPointer(p_window, this);

		if (!p_window) {
			glfwTerminate();
			SNK_BREAK("Failed to create GLFW window");
		}

		glfwSetKeyCallback(p_window, GLFW_KeyCallback);
		glfwSetFramebufferSizeCallback(p_window, FramebufferResizeCallback);
	}

	static void GLFW_KeyCallback(GLFWwindow* p_window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
			glfwSetWindowShouldClose(p_window, GLFW_TRUE);
		}
	}

	static void FramebufferResizeCallback(GLFWwindow* p_window, [[maybe_unused]] int width, [[maybe_unused]] int height) {
		auto* p_app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(p_window));
		p_app->m_framebuffer_resized = true;
	}

	SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device) {
		SwapChainSupportDetails details;
		SNK_CHECK_VK_RESULT(
			device.getSurfaceCapabilitiesKHR(*m_surface, &details.capabilities, VULKAN_HPP_DEFAULT_DISPATCHER),
			"Get device surface capabilities"
		);

		uint32_t num_formats;
		SNK_CHECK_VK_RESULT(
			device.getSurfaceFormatsKHR(*m_surface, &num_formats, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER),
			"Get device surface formats 1"
		);

		if (num_formats != 0) {
			details.formats.resize(num_formats);
			SNK_CHECK_VK_RESULT(
				device.getSurfaceFormatsKHR(*m_surface, &num_formats, details.formats.data(), VULKAN_HPP_DEFAULT_DISPATCHER),
				"Get device surface formats 2"
			);
		}

		uint32_t present_mode_count;
		SNK_CHECK_VK_RESULT(
			device.getSurfacePresentModesKHR(*m_surface, &present_mode_count, nullptr),
			"Get surface present modes 1"
		);

		if (present_mode_count != 0) {
			details.present_modes.resize(present_mode_count);
			SNK_CHECK_VK_RESULT(
				device.getSurfacePresentModesKHR(*m_surface, &present_mode_count, details.present_modes.data()),
				"Get surface present modes 2"
			);
		}

		return details;
	}

	void PickPhysicalDevice() {
		uint32_t device_count = 0;
		auto res = m_instance->enumeratePhysicalDevices(&device_count, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER);
		SNK_CHECK_VK_RESULT(res, "Physical device enumeration 1");

		if (device_count == 0)
			SNK_BREAK("No physical devices found for Vulkan, exiting");

		std::vector<vk::PhysicalDevice> devices(device_count);
		auto res2 = m_instance->enumeratePhysicalDevices(&device_count, devices.data(), VULKAN_HPP_DEFAULT_DISPATCHER);
		SNK_CHECK_VK_RESULT(res2, "Physical device enumeration 2");

		for (const auto& device : devices) {
			vk::PhysicalDeviceProperties device_properties;
			if (IsDeviceSuitable(device)) {
				m_physical_device = device;
				device.getProperties(&device_properties, VULKAN_HPP_DEFAULT_DISPATCHER);
				SNK_CORE_INFO("Suitable physical device found '{0}'", device_properties.deviceName.data());
			}
		}

		SNK_ASSERT(m_physical_device, "Physical device found");
	}

	QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device) {
		uint32_t qf_count = 0;

		device.getQueueFamilyProperties(&qf_count, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER);

		std::vector<vk::QueueFamilyProperties> queue_families(qf_count);
		device.getQueueFamilyProperties(&qf_count, queue_families.data(), VULKAN_HPP_DEFAULT_DISPATCHER);

		for (uint32_t i = 0; i < queue_families.size(); i++) {
			QueueFamilyIndices indices;
			if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics)
				indices.graphics_family = i;

			auto [result, present_support] = device.getSurfaceSupportKHR(i, m_surface.get(), VULKAN_HPP_DEFAULT_DISPATCHER);
			if (present_support)
				indices.present_family = i;

			if (indices.IsComplete())
				return indices;
		}

		return QueueFamilyIndices{};
	}

	// Choose format swap chain images use
	vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats) {
		for (const auto& format : available_formats) {
			if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
				return format;
		}

		// Pick first available as default if ideal format not found
		return available_formats[0];
	}

	// Choose way in which swap chain images are presented
	vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& available_present_modes) {
		for (const auto& mode : available_present_modes) {
			if (mode == vk::PresentModeKHR::eMailbox)
				return mode;
		}

		// FIFO guaranteed to be available so pick this as default
		return vk::PresentModeKHR::eFifo;
	}

	// Choose resolution of swap chain images
	vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		} else { // Width/height can differ from capabilities.currentExtent
			int width, height;
			glfwGetFramebufferSize(p_window, &width, &height);

			vk::Extent2D actual_extent = {
				(uint32_t)width,
				(uint32_t)height
			};
			actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actual_extent;
		}
	}

	void CreateSwapChain() {
		SwapChainSupportDetails swapchain_support = QuerySwapChainSupport(m_physical_device);

		vk::SurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(swapchain_support.formats);
		vk::PresentModeKHR present_mode = ChooseSwapPresentMode(swapchain_support.present_modes);
		vk::Extent2D extent = ChooseSwapExtent(swapchain_support.capabilities);

		uint32_t image_count = swapchain_support.capabilities.minImageCount + 1; // Add extra image to prevent having to wait on driver before acquiring another image

		// 0 means no maximum
		if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount)
			image_count = swapchain_support.capabilities.maxImageCount;

		vk::SwapchainCreateInfoKHR create_info{ vk::SwapchainCreateFlagsKHR(0),
			*m_surface,
			image_count,
			surface_format.format,
			surface_format.colorSpace,
			extent,
			1,
			vk::ImageUsageFlagBits::eColorAttachment
		};

		QueueFamilyIndices indices = FindQueueFamilies(m_physical_device);
		std::array<uint32_t, 2> qf_indices = { indices.graphics_family.value(), indices.present_family.value() };

		if (indices.graphics_family != indices.present_family) {
			create_info.imageSharingMode = vk::SharingMode::eConcurrent;
			create_info.queueFamilyIndexCount = 2;
			create_info.pQueueFamilyIndices = qf_indices.data();
		}
		else {
			create_info.imageSharingMode = vk::SharingMode::eExclusive;
			create_info.queueFamilyIndexCount = 0;
			create_info.pQueueFamilyIndices = nullptr;
		}

		create_info.preTransform = swapchain_support.capabilities.currentTransform;
		create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque; // Don't blend with other windows in window system
		create_info.presentMode = present_mode;
		create_info.clipped = VK_TRUE; // Don't care about color of pixels that are obscured by other windows
		create_info.oldSwapchain = VK_NULL_HANDLE;

		m_swapchain = m_device->createSwapchainKHRUnique(create_info).value;

		uint32_t swapchain_image_count;
		SNK_CHECK_VK_RESULT(
			m_device->getSwapchainImagesKHR(*m_swapchain, &swapchain_image_count, nullptr),
			"device->getSwapChainImagesKHR 0"
		)

		m_swapchain_images.resize(image_count);

		SNK_CHECK_VK_RESULT(
			m_device->getSwapchainImagesKHR(*m_swapchain, &swapchain_image_count, m_swapchain_images.data()),
			"device->getSwapChainImagesKHR 1"
		)

		m_swapchain_format = surface_format.format;
		m_swapchain_extent = extent;
	}

	bool IsDeviceSuitable(vk::PhysicalDevice device) {
		vk::PhysicalDeviceProperties properties;
		device.getProperties(&properties, VULKAN_HPP_DEFAULT_DISPATCHER);

		if (!CheckDeviceExtensionSupport(device))
			return false;

		SwapChainSupportDetails swapchain_support = QuerySwapChainSupport(device);
		bool swapchain_valid = !swapchain_support.formats.empty() && !swapchain_support.present_modes.empty();

		QueueFamilyIndices qf_indices = FindQueueFamilies(device);
		return properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu && qf_indices.IsComplete() && swapchain_valid;
	}

	bool CheckDeviceExtensionSupport(vk::PhysicalDevice device) {
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

		std::set<std::string> required_extensions(m_required_device_extensions.begin(), m_required_device_extensions.end());

		for (const auto& extension : available_extensions) {
			required_extensions.erase(extension.extensionName);
		}

		return required_extensions.empty();
	}

	void CreateWindowSurface() {
		VkSurfaceKHR surface_temp;
		
		auto res = vk::Result(glfwCreateWindowSurface(m_instance.get(), p_window, nullptr, &surface_temp));
		m_surface = vk::UniqueSurfaceKHR{ surface_temp, m_instance.get() };
		SNK_CHECK_VK_RESULT(res, "Window surface created");
	};

	void CreateDebugCallback() {
		vk::DebugUtilsMessageSeverityFlagsEXT severity_flags = 
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;

		vk::DebugUtilsMessageTypeFlagsEXT type_flags =
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;

		vk::DebugUtilsMessengerCreateInfoEXT messenger_create_info{
			vk::DebugUtilsMessengerCreateFlagBitsEXT(0),
			severity_flags,
			type_flags,
			&DebugCallback,
			nullptr
		};

		m_messenger = m_instance->createDebugUtilsMessengerEXTUnique(messenger_create_info, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER).value;
		SNK_ASSERT(m_messenger, "Debug messenger created");
	}

	void CreateLogicalDevice() {
		QueueFamilyIndices indices = FindQueueFamilies(m_physical_device);
		float queue_priority = 1.f;

		vk::PhysicalDeviceFeatures device_features{};

		std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
		std::set<uint32_t> unique_queue_families = { indices.graphics_family.value(), indices.present_family.value() };

		for (uint32_t queue_family : unique_queue_families) {
			vk::DeviceQueueCreateInfo queue_create_info{
			vk::DeviceQueueCreateFlags(0),
			queue_family,
			1,
			&queue_priority,
			nullptr
			};

			queue_create_infos.push_back(queue_create_info);
		}

		vk::PhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features{};
		dynamic_rendering_features.dynamicRendering = VK_TRUE;

		auto device_create_info = vk::DeviceCreateInfo{}
			.setQueueCreateInfoCount((uint32_t)queue_create_infos.size())
			.setPQueueCreateInfos(queue_create_infos.data())
			.setEnabledExtensionCount((uint32_t)m_required_device_extensions.size())
			.setPpEnabledExtensionNames(m_required_device_extensions.data())
			.setPNext(&dynamic_rendering_features)
			.setPEnabledFeatures(&device_features);
			

		m_device = m_physical_device.createDeviceUnique(device_create_info, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER).value;
		SNK_ASSERT(m_device, "Logical device creation");

		m_graphics_queue = m_device->getQueue(indices.graphics_family.value(), 0, VULKAN_HPP_DEFAULT_DISPATCHER);
		m_presentation_queue = m_device->getQueue(indices.present_family.value(), 0, VULKAN_HPP_DEFAULT_DISPATCHER);
	}

	void CreateInstance(const char* app_name) {
		std::vector<const char*> layers = {
			"VK_LAYER_KHRONOS_validation"
		};
		
		std::vector<const char*> extensions = {
			VK_KHR_SURFACE_EXTENSION_NAME,
#if defined (_WIN32)
			"VK_KHR_win32_surface",
#endif
#if defined (__APPLE__)
			"VK_MVK_macos_surface"
#endif
#if defined (__linux__)
			"VK_KHR_xcb_surface"
#endif
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		};

		vk::ApplicationInfo app_info{ 
			app_name,
			VK_MAKE_API_VERSION(0, 1, 0, 0),
			"Snake",
			VK_MAKE_API_VERSION(0, 1, 0, 0),
			VK_API_VERSION_1_0
		};

		vk::InstanceCreateInfo create_info{ 
			vk::Flags<vk::InstanceCreateFlagBits>(0),
			&app_info,
			{(uint32_t)layers.size(), layers.data()},
			{(uint32_t)extensions.size(), extensions.data()}
		};

		m_instance = vk::createInstanceUnique(create_info, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER).value;
		SNK_ASSERT(m_instance, "Vulkan instance created");
	}
	
	void CreateImageViews() {
		m_swapchain_image_views.resize(m_swapchain_images.size());

		for (int i = 0; i < m_swapchain_images.size(); i++) {
			vk::ImageViewCreateInfo create_info{ vk::ImageViewCreateFlags{0}, m_swapchain_images[i], vk::ImageViewType::e2D, m_swapchain_format };
			create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			create_info.subresourceRange.baseMipLevel = 0;
			create_info.subresourceRange.levelCount = 1;
			create_info.subresourceRange.baseArrayLayer = 0;
			create_info.subresourceRange.layerCount = 1;

			m_swapchain_image_views[i] = m_device->createImageViewUnique(create_info).value;

			SNK_ASSERT(m_swapchain_image_views[i], "Swapchain image view {0} created", i);
		}
	}

	void CreateGraphicsPipeline() {
		constexpr char vert_fp[] = "res/shaders/vert.spv";
		constexpr char frag_fp[] = "res/shaders/frag.spv";

		auto vert_code = ReadFileBinary(vert_fp);
		auto frag_code = ReadFileBinary(frag_fp);

		SNK_ASSERT(!vert_code.empty(), "Vertex shader code loaded");
		SNK_ASSERT(!frag_code.empty(), "Fragment shader code loaded");

		auto vert_module = CreateShaderModule(vert_code, vert_fp);
		auto frag_module = CreateShaderModule(frag_code, frag_fp);

		vk::PipelineShaderStageCreateInfo vert_info{ vk::PipelineShaderStageCreateFlags{0}, vk::ShaderStageFlagBits::eVertex, vert_module.get(), "main"};
		vk::PipelineShaderStageCreateInfo frag_info{ vk::PipelineShaderStageCreateFlags{0}, vk::ShaderStageFlagBits::eFragment, frag_module.get(), "main"};
		vk::PipelineShaderStageCreateInfo shader_stages[] = {vert_info, frag_info};

		auto binding_desc = Vertex::GetBindingDescription();
		auto attribute_desc = Vertex::GetAttributeDescriptions();

		// Stores vertex layout data
		vk::PipelineVertexInputStateCreateInfo vert_input_info{ vk::PipelineVertexInputStateCreateFlags{0}, nullptr, {} };
		vert_input_info.vertexBindingDescriptionCount = 1;
		vert_input_info.vertexAttributeDescriptionCount = (uint32_t)attribute_desc.size();
		vert_input_info.pVertexBindingDescriptions = &binding_desc;
		vert_input_info.pVertexAttributeDescriptions = attribute_desc.data();

		// Stores how vertex data is interpreted by input assembler
		vk::PipelineInputAssemblyStateCreateInfo input_assembly_create_info{ 
			vk::PipelineInputAssemblyStateCreateFlags{0}, vk::PrimitiveTopology::eTriangleList, VK_FALSE };

		vk::Viewport viewport{ 0.f, 0.f, (float)m_swapchain_extent.width, (float)m_swapchain_extent.height, 0.f, 1.f };
		// Scissor region is region in which pixels are drawn, in this case the whole framebuffer
		vk::Rect2D scissor{ {0, 0}, m_swapchain_extent };

		std::vector<vk::DynamicState> dynamic_states = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};

		vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{ vk::PipelineDynamicStateCreateFlags{0}, dynamic_states };

		vk::PipelineViewportStateCreateInfo viewport_create_info{}; // Don't set pviewport or pscissor to keep this dynamic
		viewport_create_info.viewportCount = 1;
		viewport_create_info.scissorCount = 1;

		vk::PipelineRasterizationStateCreateInfo rasterizer_create_info{};
		rasterizer_create_info.depthClampEnable = VK_FALSE; // if fragments beyond near and far plane are clamped to them instead of discarding
		rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE; // if geometry should never pass through rasterizer stage
		rasterizer_create_info.polygonMode = vk::PolygonMode::eFill;
		rasterizer_create_info.lineWidth = 1.f;
		rasterizer_create_info.cullMode = vk::CullModeFlagBits::eBack;
		rasterizer_create_info.frontFace = vk::FrontFace::eClockwise;
		rasterizer_create_info.depthBiasEnable = VK_FALSE;

		vk::PipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
		multisampling.minSampleShading = 1.f;

		vk::PipelineColorBlendAttachmentState colour_blend_attachment{};
		// Enable alpha blending
		colour_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		colour_blend_attachment.blendEnable = VK_TRUE;
		colour_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
		colour_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
		colour_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
		colour_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
		colour_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
		colour_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;

		vk::PipelineColorBlendStateCreateInfo colour_blend_state;
		colour_blend_state.logicOpEnable = VK_FALSE;
		colour_blend_state.attachmentCount = 1;
		colour_blend_state.pAttachments = &colour_blend_attachment;
		
		// Use to attach descriptor sets
		vk::PipelineLayoutCreateInfo pipeline_layout_info{};
		//pipeline_layout_info.setLayoutCount = 1;
		//pipeline_layout_info.pSetLayouts = &*m_descriptor_set_layout;

		m_pipeline_layout = m_device->createPipelineLayoutUnique(pipeline_layout_info).value;

		SNK_ASSERT(m_pipeline_layout, "Graphics pipeline layout created");

		std::vector<vk::Format> formats = { m_swapchain_format };
		vk::PipelineRenderingCreateInfo render_info{};
		render_info.colorAttachmentCount = 1;
		render_info.pColorAttachmentFormats = formats.data();

		vk::GraphicsPipelineCreateInfo pipeline_info{};
		pipeline_info.stageCount = 2;
		pipeline_info.pStages = shader_stages;
		pipeline_info.pVertexInputState = &vert_input_info;
		pipeline_info.pInputAssemblyState = &input_assembly_create_info;
		pipeline_info.pViewportState = &viewport_create_info;
		pipeline_info.pRasterizationState = &rasterizer_create_info;
		pipeline_info.pMultisampleState = &multisampling;
		pipeline_info.pDepthStencilState = nullptr;
		pipeline_info.pColorBlendState = &colour_blend_state;
		pipeline_info.pDynamicState = &dynamic_state_create_info;
		pipeline_info.layout = *m_pipeline_layout;
		pipeline_info.renderPass = nullptr;
		pipeline_info.subpass = 0; // index of subpass where this pipeline is used
		pipeline_info.pNext = &render_info;

		auto [result, pipeline] = m_device->createGraphicsPipelineUnique(VK_NULL_HANDLE, pipeline_info).asTuple();
		m_graphics_pipeline = std::move(pipeline);

		SNK_CHECK_VK_RESULT(result, "Graphics pipeline result successful");
		SNK_ASSERT(m_graphics_pipeline, "Graphics pipeline successfully created");
	}


	void CreateCommandPool() {
		QueueFamilyIndices qf_indices = FindQueueFamilies(m_physical_device);

		vk::CommandPoolCreateInfo pool_info{};
		pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer; // allow command buffers to be rerecorded individually instead of having to be reset together
		pool_info.queueFamilyIndex = qf_indices.graphics_family.value();

		m_cmd_pool = m_device->createCommandPoolUnique(pool_info).value;
		SNK_ASSERT(m_cmd_pool, "Command pool created");
	}

	void CreateCommandBuffers() {
		vk::CommandBufferAllocateInfo alloc_info{};
		alloc_info.commandPool = *m_cmd_pool;
		alloc_info.level = vk::CommandBufferLevel::ePrimary; // can be submitted to a queue for execution, can't be called by other command buffers

		m_cmd_buffers.resize(MAX_FRAMES_IN_FLIGHT);
		alloc_info.commandBufferCount = (uint32_t)m_cmd_buffers.size();
		
		m_cmd_buffers = std::move(m_device->allocateCommandBuffersUnique(alloc_info).value);
		SNK_ASSERT(m_cmd_buffers[0], "Command buffers created");
	}

	void RecordCommandBuffer(vk::CommandBuffer cmd_buffer, uint32_t image_index) {
		vk::CommandBufferBeginInfo begin_info{};

		SNK_CHECK_VK_RESULT(
			m_cmd_buffers[m_current_frame]->begin(begin_info),
			"Begin recording command buffer"
		);

		// Transition image layout from presentSrc to colour
		vk::ImageMemoryBarrier image_mem_barrier{};
		image_mem_barrier.oldLayout = vk::ImageLayout::eUndefined;
		image_mem_barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
		image_mem_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		image_mem_barrier.image = m_swapchain_images[image_index];
		image_mem_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		image_mem_barrier.subresourceRange.baseMipLevel = 0;
		image_mem_barrier.subresourceRange.levelCount = 1;
		image_mem_barrier.subresourceRange.baseArrayLayer = 0;
		image_mem_barrier.subresourceRange.layerCount = 1;

		cmd_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::DependencyFlags{ 0 }, {}, {}, image_mem_barrier
		);

		vk::RenderingAttachmentInfo colour_attachment_info{};
		colour_attachment_info.loadOp = vk::AttachmentLoadOp::eClear; // Clear initially
		colour_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
		colour_attachment_info.imageView = *m_swapchain_image_views[image_index];
		colour_attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;

		vk::RenderingInfo render_info{};
		render_info.layerCount = 1;
		render_info.colorAttachmentCount = 1;
		render_info.pColorAttachments = &colour_attachment_info;
		render_info.renderArea.extent = m_swapchain_extent;
		render_info.renderArea.offset = vk::Offset2D{ 0, 0 };


		cmd_buffer.beginRenderingKHR(
			render_info
		);

		cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphics_pipeline);

		vk::Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(m_swapchain_extent.width);
		viewport.height = static_cast<float>(m_swapchain_extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd_buffer.setViewport(0, 1, &viewport);

		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = m_swapchain_extent;
		cmd_buffer.setScissor(0, 1, &scissor);

		std::vector<vk::Buffer> vert_buffers = { *m_vertex_buffer };
		std::vector<vk::Buffer> index_buffers = { *m_index_buffer };
		std::vector<vk::DeviceSize> offsets = { 0 };
		cmd_buffer.bindVertexBuffers(0, 1, vert_buffers.data(), offsets.data());
		cmd_buffer.bindIndexBuffer(*m_index_buffer, 0, vk::IndexType::eUint16);


		cmd_buffer.drawIndexed((uint32_t)m_indices.size(), 1, 0, 0, 0);

		cmd_buffer.endRenderingKHR();

		// Transition image layout from colour to presentSrc
		vk::ImageMemoryBarrier image_mem_barrier_2{};
		image_mem_barrier_2.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		image_mem_barrier_2.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
		image_mem_barrier_2.newLayout = vk::ImageLayout::ePresentSrcKHR;
		image_mem_barrier_2.image = m_swapchain_images[image_index];
		image_mem_barrier_2.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		image_mem_barrier_2.subresourceRange.baseMipLevel = 0;
		image_mem_barrier_2.subresourceRange.levelCount = 1;
		image_mem_barrier_2.subresourceRange.baseArrayLayer = 0;
		image_mem_barrier_2.subresourceRange.layerCount = 1;

		cmd_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::DependencyFlags{ 0 }, {}, {}, image_mem_barrier_2
		);


		SNK_CHECK_VK_RESULT(
			cmd_buffer.end(),
			"End recording command buffer"
		);
	}

	vk::UniqueShaderModule CreateShaderModule(const std::vector<char>& code, const std::string& filepath) {
		vk::ShaderModuleCreateInfo create_info{
			vk::ShaderModuleCreateFlags{0},
			*reinterpret_cast<const std::vector<uint32_t>*>(&code)
		};

		vk::UniqueShaderModule shader_module = m_device->createShaderModuleUnique(create_info).value;

		SNK_ASSERT(shader_module, "Shader module for shader '{0}' created successfully", filepath)
		return shader_module;
	}

	void CreateSyncObjects() {
		vk::SemaphoreCreateInfo semaphore_info{};
		vk::FenceCreateInfo fence_info{};
		fence_info.flags = vk::FenceCreateFlagBits::eSignaled; // Create in initially signalled state so execution begins immediately


		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			m_image_avail_semaphores.push_back(std::move(m_device->createSemaphoreUnique(semaphore_info).value));
			m_render_finished_semaphores.push_back(std::move(m_device->createSemaphoreUnique(semaphore_info).value));
			m_in_flight_fences.push_back(std::move(m_device->createFenceUnique(fence_info).value));
		}

		SNK_ASSERT(m_image_avail_semaphores[0] && m_render_finished_semaphores[0] && m_in_flight_fences[0], "Synchronization objects created");
	}

	void RenderLoop() {
		while (!glfwWindowShouldClose(p_window)) {
			DrawFrame();
			glfwPollEvents();
		}

		glfwTerminate();
	}

	void DrawFrame() {
		SNK_CHECK_VK_RESULT(
			m_device->waitForFences(1, &*m_in_flight_fences[m_current_frame], VK_TRUE, UINT64_MAX),
			"DrawFrame waitForFences"
		);

		uint32_t image_index;
		auto image_result = m_device->acquireNextImageKHR(*m_swapchain, UINT64_MAX, *m_image_avail_semaphores[m_current_frame], VK_NULL_HANDLE, &image_index);

		// Swap chain suddenly incompatible with surface, likely a window resize
		if (image_result == vk::Result::eErrorOutOfDateKHR) {
			RecreateSwapChain();
			return;
		}
		else if (image_result != vk::Result::eSuccess) {
			SNK_BREAK("Failed to acquire swap chain image");
		}

		SNK_CHECK_VK_RESULT(
			m_device->resetFences(1, &*m_in_flight_fences[m_current_frame]),
			"DrawFrame resetFences"
		);

		m_cmd_buffers[m_current_frame]->reset();
		RecordCommandBuffer(*m_cmd_buffers[m_current_frame], image_index);

		vk::SubmitInfo submit_info{};
		vk::Semaphore wait_semaphores[] = {*m_image_avail_semaphores[m_current_frame]}; // Which semaphores to wait on
		vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput }; // Which stages of the pipeline to wait in
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &*m_cmd_buffers[m_current_frame];

		vk::Semaphore signal_semaphores[] = { *m_render_finished_semaphores[m_current_frame] }; // Which semaphores to signal once command buffer(s) have finished execution
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;

		// Queue work to draw triangle, triggers the fence to be active when it starts
		SNK_CHECK_VK_RESULT(
			m_graphics_queue.submit(1, &submit_info, *m_in_flight_fences[m_current_frame]), // Fence is signalled once command buffer finishes execution
			"DrawFrame submit to graphics queue"
		);

		vk::SwapchainKHR swapchains[] = {*m_swapchain};

		vk::PresentInfoKHR present_info{};
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swapchains;
		present_info.pImageIndices = &image_index;
		
		// Queue presentation work that waits on the render_finished semaphore
		auto present_result = m_presentation_queue.presentKHR(present_info);
		if (present_result == vk::Result::eErrorOutOfDateKHR || m_framebuffer_resized || present_result == vk::Result::eSuboptimalKHR) {
			m_framebuffer_resized = false;
			RecreateSwapChain();
		}
		else if (present_result != vk::Result::eSuccess) {
			SNK_BREAK("Failed to present image");
		}

		m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void RecreateSwapChain() {
		SNK_CHECK_VK_RESULT(
			m_device->waitIdle(),
			"WaitIdle"
		);

		m_swapchain.reset();
		m_swapchain_images.clear();
		m_swapchain_image_views.clear();

		CreateSwapChain();
		CreateImageViews();
	}

	void CreateVertexBuffer() {
		std::vector<Vertex> vertices = {
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
		};

		vk::UniqueBuffer staging_buffer;
		vk::UniqueDeviceMemory staging_buffer_mem;

		size_t size = vertices.size() * sizeof(Vertex);

		// Create staging buffer to load data in from CPU, then transfer to a more optimized buffer only available on the GPU.
		CreateBuffer(size, vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, staging_buffer, staging_buffer_mem);

		void* data = nullptr;
		SNK_CHECK_VK_RESULT(
			m_device->mapMemory(*staging_buffer_mem, 0, size, {}, &data),
			"Vertex buffer memory mapping"
		);
		memcpy(data, vertices.data(), size);
		m_device->unmapMemory(*staging_buffer_mem);

		CreateBuffer(size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal, m_vertex_buffer, m_vertex_buffer_memory);

		CopyBuffer(*staging_buffer, *m_vertex_buffer, size);
	}

	void CreateIndexBuffer() {
		vk::UniqueBuffer staging_buffer;
		vk::UniqueDeviceMemory staging_buffer_mem;

		size_t size = m_indices.size() * sizeof(Vertex);

		// Create staging buffer to load data in from CPU, then transfer to a more optimized buffer only available on the GPU.
		CreateBuffer(size, vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, staging_buffer, staging_buffer_mem);

		void* data = nullptr;
		SNK_CHECK_VK_RESULT(
			m_device->mapMemory(*staging_buffer_mem, 0, size, {}, &data),
			"Vertex buffer memory mapping"
		);
		memcpy(data, m_indices.data(), size);
		m_device->unmapMemory(*staging_buffer_mem);

		CreateBuffer(size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal, m_index_buffer, m_index_buffer_memory);

		CopyBuffer(*staging_buffer, *m_index_buffer, size);
	}

	void CopyBuffer(vk::Buffer& src, vk::Buffer dst, vk::DeviceSize size) {
		vk::CommandBufferAllocateInfo alloc_info{};
		alloc_info.level = vk::CommandBufferLevel::ePrimary;
		alloc_info.commandPool = *m_cmd_pool;
		alloc_info.commandBufferCount = 1;

		// Create new command buffer to perform memory transfer
		auto [result, bufs] = m_device->allocateCommandBuffersUnique(alloc_info);
		SNK_CHECK_VK_RESULT(
			result,
			"CopyBuffer command buffer allocation"
		);

		vk::UniqueCommandBuffer cmd_buf = std::move(bufs[0]);

		vk::CommandBufferBeginInfo begin_info{};
		begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

		// Start recording
		SNK_CHECK_VK_RESULT(
			cmd_buf->begin(begin_info),
			"CopyBuffer command buffer begin()"
		);

		vk::BufferCopy copy_region{};
		copy_region.srcOffset = 0;
		copy_region.dstOffset = 0;
		copy_region.size = size;

		cmd_buf->copyBuffer(src, dst, copy_region);

		// Stop recording
		SNK_CHECK_VK_RESULT(
			cmd_buf->end(),
			"CopyBuffer command buffer end()"
		);

		vk::SubmitInfo submit_info{};
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &*cmd_buf;
		
		// Graphics queues implicitly support queue transfer operations, don't need special transfer queue
		SNK_CHECK_VK_RESULT(
			m_graphics_queue.submit(submit_info),
			"Graphics queue transfer operation submission"
		);

		// Wait for transfer queue to become idle, this can be sped up with fences as multiple transfer ops can occur simultaneously
		SNK_CHECK_VK_RESULT(m_graphics_queue.waitIdle(), "CopyBuffer graphics queue waitIdle");
	}

	void CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::UniqueBuffer& buffer, vk::UniqueDeviceMemory& buffer_memory) {
		vk::BufferCreateInfo buffer_info{};
		buffer_info.size = size;
		buffer_info.usage = usage;
		buffer_info.sharingMode = vk::SharingMode::eExclusive;

		auto [result, new_buffer] = m_device->createBufferUnique(buffer_info);
		SNK_CHECK_VK_RESULT(result, "Buffer creation");
		buffer = std::move(new_buffer);

		vk::MemoryRequirements mem_requirements = m_device->getBufferMemoryRequirements(*buffer);

		vk::MemoryAllocateInfo alloc_info{};
		alloc_info.allocationSize = mem_requirements.size;
		alloc_info.memoryTypeIndex = FindMemoryType(mem_requirements.memoryTypeBits, properties);

		auto [alloc_result, mem] = m_device->allocateMemoryUnique(alloc_info);
		SNK_CHECK_VK_RESULT(alloc_result, "Buffer memory allocation");

		buffer_memory = std::move(mem);

		SNK_CHECK_VK_RESULT(
			m_device->bindBufferMemory(*buffer, *buffer_memory, 0),
			"Buffer memory binding"
		);
	}

	uint32_t FindMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties) {
		auto phys_mem_properties = m_physical_device.getMemoryProperties();

		for (uint32_t i = 0; i < phys_mem_properties.memoryTypeCount; i++) {
			if (type_filter & (1 << i) && 
				(static_cast<uint32_t>(phys_mem_properties.memoryTypes[i].propertyFlags) & static_cast<uint32_t>(properties)) == static_cast<uint32_t>(properties)) {
				return i;
			}
		}

		SNK_BREAK("Failed to find memory type");
		return 0;
	}

	void CreateDescriptorSetLayout() {
		vk::DescriptorSetLayoutBinding ubo_layout_binding{};
		ubo_layout_binding.binding = 0;
		ubo_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
		ubo_layout_binding.descriptorCount = 1;
		ubo_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex; // Descriptor only referenced in vertex stage

		vk::DescriptorSetLayoutCreateInfo layout_info{};
		layout_info.bindingCount = 1;
		layout_info.pBindings = &ubo_layout_binding;

		auto [res, val] = m_device->createDescriptorSetLayoutUnique(layout_info);
		SNK_CHECK_VK_RESULT(res, "Create descriptor set layout");
	
		m_descriptor_set_layout = std::move(val);
	}

	~VulkanApp() {
		// Wait for any fences before destroying them
		for (auto& fence : m_in_flight_fences) {
			SNK_CHECK_VK_RESULT(
				m_device->waitForFences(*fence, true, std::numeric_limits<uint64_t>::max()),
				"Cleanup - waiting for fences"
			)
		}
	}

	const std::vector<uint16_t> m_indices = {
		0, 1, 2, 2, 3, 0
	};

	GLFWwindow* p_window = nullptr;


	vk::UniqueBuffer m_vertex_buffer;
	vk::UniqueDeviceMemory m_vertex_buffer_memory;

	vk::UniqueBuffer m_index_buffer;
	vk::UniqueDeviceMemory m_index_buffer_memory;

	vk::UniqueInstance m_instance;
	vk::UniqueDebugUtilsMessengerEXT m_messenger;
	vk::UniqueSurfaceKHR m_surface;
	vk::PhysicalDevice m_physical_device;
	vk::UniqueDevice m_device;
	vk::Queue m_graphics_queue;
	vk::Queue m_presentation_queue;
	vk::UniqueSwapchainKHR m_swapchain;

	std::vector<vk::Image> m_swapchain_images;
	std::vector<vk::UniqueImageView> m_swapchain_image_views;

	vk::Format m_swapchain_format;
	vk::Extent2D m_swapchain_extent;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniquePipelineLayout m_pipeline_layout;

	vk::UniquePipeline m_graphics_pipeline;

	vk::UniqueCommandPool m_cmd_pool;
	std::vector<vk::UniqueCommandBuffer> m_cmd_buffers;

	std::vector<vk::UniqueSemaphore> m_image_avail_semaphores;
	std::vector<vk::UniqueSemaphore> m_render_finished_semaphores;
	std::vector<vk::UniqueFence> m_in_flight_fences;

	bool m_framebuffer_resized = false;

	uint32_t m_current_frame = 0;

	static constexpr std::array<const char*, 4> m_required_device_extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
		VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
		VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
	};

};


int main() {
	std::filesystem::current_path("../../../../Core");
	SNAKE::Logger::Init();

	VulkanApp app;
	app.Init("Snake");
	app.RenderLoop();

	return 0;
}