#include <iostream>
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "util.h"
#include "FileUtil.h"
#include <filesystem>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using namespace SNAKE;

void GLFW_KeyCallback(GLFWwindow* p_window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(p_window, GLFW_TRUE);
	}
}

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

class Window {
public:
	void Init() {
		// Default API is OpenGL so set to GLFW_NO_API
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, false);

		p_window = glfwCreateWindow(1920, 1080, "ORNG VK", nullptr, nullptr);
		if (!p_window) {
			glfwTerminate();
			SNK_BREAK("Failed to create GLFW window");
		}

		glfwSetKeyCallback(p_window, GLFW_KeyCallback);
	}

	GLFWwindow* GetWindow() {
		return p_window;
	}

private:
	GLFWwindow* p_window = nullptr;
};

class VulkanApp {
public:
	void Init(const char* app_name, GLFWwindow* p_window) {
		VULKAN_HPP_DEFAULT_DISPATCHER.init();

		CreateInstance(app_name); 

		VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance.get());

		CreateDebugCallback();
		CreateWindowSurface(p_window);
		PickPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain(p_window);
		CreateImageViews();
		CreateGraphicsPipeline();
		CreateFramebuffers();
		CreateCommandPool();
		CreateCommandBuffer();
		CreateSyncObjects();

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

			vk::Bool32 present_support = device.getSurfaceSupportKHR(i, m_surface.get(), VULKAN_HPP_DEFAULT_DISPATCHER);
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
	vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* p_window) {
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

	void CreateSwapChain(GLFWwindow* p_window) {
		SwapChainSupportDetails swapchain_support = QuerySwapChainSupport(m_physical_device);

		vk::SurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(swapchain_support.formats);
		vk::PresentModeKHR present_mode = ChooseSwapPresentMode(swapchain_support.present_modes);
		vk::Extent2D extent = ChooseSwapExtent(swapchain_support.capabilities, p_window);

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

		m_swapchain = m_device->createSwapchainKHRUnique(create_info);

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

	void CreateWindowSurface(GLFWwindow* p_window) {
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

		m_messenger = m_instance->createDebugUtilsMessengerEXTUnique(messenger_create_info, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER);
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

		vk::DeviceCreateInfo device_create_info{ vk::DeviceCreateFlags(0),
			{(uint32_t)queue_create_infos.size(), queue_create_infos.data()},
			{},
			{(uint32_t)m_required_device_extensions.size(), m_required_device_extensions.data()},
			&device_features
		};

		m_device = m_physical_device.createDeviceUnique(device_create_info, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER);
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

		m_instance = vk::createInstanceUnique(create_info, nullptr, VULKAN_HPP_DEFAULT_DISPATCHER);
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

			m_swapchain_image_views[i] = m_device->createImageViewUnique(create_info);

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

		// Stores vertex data
		vk::PipelineVertexInputStateCreateInfo vert_input_state_create_info{ vk::PipelineVertexInputStateCreateFlags{0}, nullptr, {} };

		// Stores how vertex data is interpreted
		vk::PipelineInputAssemblyStateCreateInfo input_assembly_create_info{ vk::PipelineInputAssemblyStateCreateFlags{0}, vk::PrimitiveTopology::eTriangleList, VK_FALSE };

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

		
		vk::PipelineLayoutCreateInfo pipeline_layout_info{};
		pipeline_layout_info.setLayoutCount = 0;

		m_pipeline_layout = m_device->createPipelineLayoutUnique(pipeline_layout_info);

		SNK_ASSERT(m_pipeline_layout, "Graphics pipeline layout created");

		CreateRenderpass();

		vk::GraphicsPipelineCreateInfo pipeline_info{};
		pipeline_info.stageCount = 2;
		pipeline_info.pStages = shader_stages;
		pipeline_info.pVertexInputState = &vert_input_state_create_info;
		pipeline_info.pInputAssemblyState = &input_assembly_create_info;
		pipeline_info.pViewportState = &viewport_create_info;
		pipeline_info.pRasterizationState = &rasterizer_create_info;
		pipeline_info.pMultisampleState = &multisampling;
		pipeline_info.pDepthStencilState = nullptr;
		pipeline_info.pColorBlendState = &colour_blend_state;
		pipeline_info.pDynamicState = &dynamic_state_create_info;
		pipeline_info.layout = *m_pipeline_layout;
		pipeline_info.renderPass = *m_renderpass;
		pipeline_info.subpass = 0; // index of subpass where this pipeline is used

		auto [result, pipeline] = m_device->createGraphicsPipelineUnique(VK_NULL_HANDLE, pipeline_info).asTuple();
		m_graphics_pipeline = std::move(pipeline);

		SNK_CHECK_VK_RESULT(result, "Graphics pipeline result successful");
		SNK_ASSERT(m_graphics_pipeline, "Graphics pipeline successfully created");
	}

	void CreateRenderpass() {
		// Setup framebuffer attachments
		vk::AttachmentDescription colour_attachment{};
		colour_attachment.format = m_swapchain_format;
		colour_attachment.samples = vk::SampleCountFlagBits::e1;
		colour_attachment.loadOp = vk::AttachmentLoadOp::eClear; // clear values at start
		colour_attachment.storeOp = vk::AttachmentStoreOp::eStore; // rendered contents are stored in memory to be read later
		colour_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		colour_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare; // values undefined after rendering
		colour_attachment.initialLayout = vk::ImageLayout::eUndefined; // don't care what previous layout of the image was, it's cleared at the start anyway
		colour_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR; // image will be presented to swapchain, this value should be based on what the image is going to be used for next

		vk::AttachmentReference colour_attachment_ref{};
		colour_attachment_ref.attachment = 0; // which attachment to reference by its index in the attachment descriptions array
		colour_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal; // the layout the attachment image has when this subpass starts

		vk::SubpassDescription subpass{};
		subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colour_attachment_ref; // Index of colour attachment referenced directly in shader with layout(location = 0)

		vk::RenderPassCreateInfo renderpass_info{};
		renderpass_info.attachmentCount = 1;
		renderpass_info.pAttachments = &colour_attachment;
		renderpass_info.subpassCount = 1;
		renderpass_info.pSubpasses = &subpass;

		vk::SubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // Implicit subpass before renderpass (would be after if specified in dstSubpass)
		dependency.dstSubpass = 0;
		dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dependency.srcAccessMask = vk::AccessFlagBits::eNone;
		dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

		renderpass_info.dependencyCount = 1;
		renderpass_info.pDependencies = &dependency;

		m_renderpass = m_device->createRenderPassUnique(renderpass_info);
		SNK_ASSERT(m_renderpass, "Renderpass created");

	}

	void CreateFramebuffers() {
		m_swapchain_framebuffers.resize(m_swapchain_image_views.size());

		for (size_t i = 0; i < m_swapchain_framebuffers.size(); i++) {
			vk::ImageView attachments[] = {
				*m_swapchain_image_views[i]
			};

			vk::FramebufferCreateInfo framebuffer_info{};
			framebuffer_info.renderPass = *m_renderpass;
			framebuffer_info.attachmentCount = 1;
			framebuffer_info.pAttachments = attachments;
			framebuffer_info.width = m_swapchain_extent.width;
			framebuffer_info.height = m_swapchain_extent.height;
			framebuffer_info.layers = 1;

			m_swapchain_framebuffers[i] = std::move(m_device->createFramebufferUnique(framebuffer_info));

			SNK_ASSERT(m_swapchain_framebuffers[i], "Framebuffer '{0}' created", i);
		}
	}

	void CreateCommandPool() {
		QueueFamilyIndices qf_indices = FindQueueFamilies(m_physical_device);

		vk::CommandPoolCreateInfo pool_info{};
		pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer; // allow command buffers to be rerecorded individually instead of having to be reset together
		pool_info.queueFamilyIndex = qf_indices.graphics_family.value();

		m_cmd_pool = m_device->createCommandPoolUnique(pool_info);
		SNK_ASSERT(m_cmd_pool, "Command pool created");
	}

	void CreateCommandBuffer() {
		vk::CommandBufferAllocateInfo alloc_info{};
		alloc_info.commandPool = *m_cmd_pool;
		alloc_info.level = vk::CommandBufferLevel::ePrimary; // can be submitted to a queue for execution, can't be called by other command buffers
		alloc_info.commandBufferCount = 1;

		m_cmd_buffer = std::move(m_device->allocateCommandBuffersUnique(alloc_info)[0]);
		SNK_ASSERT(m_cmd_buffer, "Command buffer created");
	}

	void RecordCommandBuffer(vk::CommandBuffer cmd_buffer, uint32_t image_index) {
		vk::CommandBufferBeginInfo begin_info{};
		m_cmd_buffer->begin(begin_info);

		vk::RenderPassBeginInfo renderpass_info{};
		renderpass_info.renderPass = *m_renderpass;
		renderpass_info.framebuffer = *m_swapchain_framebuffers[image_index];
		renderpass_info.renderArea.offset = vk::Offset2D{ 0, 0 };
		renderpass_info.renderArea.extent = m_swapchain_extent;
		renderpass_info.clearValueCount = 1;
		vk::ClearValue clear_col = { { 0.f, 0.f, 0.f, 1.f } };
		renderpass_info.pClearValues = &clear_col;

		cmd_buffer.beginRenderPass(
			renderpass_info, 
			vk::SubpassContents::eInline // renderpass commands embedded in primary command buffer, no secondary command buffers will be executed
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

		cmd_buffer.draw(3, 1, 0, 0);

		cmd_buffer.endRenderPass();
		cmd_buffer.end();
	}

	vk::UniqueShaderModule CreateShaderModule(const std::vector<char>& code, const std::string& filepath) {
		vk::ShaderModuleCreateInfo create_info{
			vk::ShaderModuleCreateFlags{0},
			*reinterpret_cast<const std::vector<uint32_t>*>(&code)
		};

		vk::UniqueShaderModule shader_module = m_device->createShaderModuleUnique(create_info);

		SNK_ASSERT(shader_module, "Shader module for shader '{0}' created successfully", filepath)
		return shader_module;
	}

	void CreateSyncObjects() {
		vk::SemaphoreCreateInfo semaphore_info{};
		vk::FenceCreateInfo fence_info{};
		fence_info.flags = vk::FenceCreateFlagBits::eSignaled; // Create in initially signalled state so execution begins immediately

		m_image_avail_semaphore = m_device->createSemaphoreUnique(semaphore_info);
		m_render_finished_semaphore = m_device->createSemaphoreUnique(semaphore_info);
		m_in_flight_fence = m_device->createFenceUnique(fence_info);

		SNK_ASSERT(m_image_avail_semaphore && m_render_finished_semaphore && m_in_flight_fence, "Synchronization objects created");
	}

	void RenderLoop() {

	}

	void DrawFrame() {
		
		SNK_CHECK_VK_RESULT(
			m_device->waitForFences(1, &*m_in_flight_fence, VK_TRUE, UINT64_MAX),
			"DrawFrame waitForFences"
		);

		SNK_CHECK_VK_RESULT(
			m_device->resetFences(1, &*m_in_flight_fence),
			"DrawFrame resetFences"
		);

		uint32_t image_index;
		
		SNK_CHECK_VK_RESULT(
			m_device->acquireNextImageKHR(*m_swapchain, UINT64_MAX, *m_image_avail_semaphore, VK_NULL_HANDLE, &image_index),
			"DrawFrame acquireNextImage"
		);

		m_cmd_buffer->reset();
		RecordCommandBuffer(*m_cmd_buffer, image_index);

		vk::SubmitInfo submit_info{};
		vk::Semaphore wait_semaphores[] = {*m_image_avail_semaphore}; // Which semaphores to wait on
		vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput }; // Which stages of the pipeline to wait in
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &*m_cmd_buffer;

		vk::Semaphore signal_semaphores[] = { *m_render_finished_semaphore }; // Which semaphores to signal once command buffer(s) have finished execution
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;

		SNK_CHECK_VK_RESULT(
			m_graphics_queue.submit(1, &submit_info, *m_in_flight_fence), // Fence is signalled once command buffer finishes execution
			"DrawFrame submit to graphics queue"
		);

		vk::SwapchainKHR swapchains[] = {*m_swapchain};

		vk::PresentInfoKHR present_info{};
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swapchains;
		present_info.pImageIndices = &image_index;
		
		SNK_CHECK_VK_RESULT(
			m_presentation_queue.presentKHR(present_info),
			"DrawFrame presentKHR"
		);

	}

	~VulkanApp() {

	}

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

	vk::UniquePipelineLayout m_pipeline_layout;
	vk::UniqueRenderPass m_renderpass;
	vk::UniquePipeline m_graphics_pipeline;

	std::vector<vk::UniqueFramebuffer> m_swapchain_framebuffers;

	vk::UniqueCommandPool m_cmd_pool;
	vk::UniqueCommandBuffer m_cmd_buffer;

	vk::UniqueSemaphore m_image_avail_semaphore;
	vk::UniqueSemaphore m_render_finished_semaphore;
	vk::UniqueFence m_in_flight_fence;

	static constexpr std::array<const char*, 1> m_required_device_extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

};


int main() {
	std::filesystem::current_path("../../../../Core");
	SNAKE::Logger::Init();

	if (!glfwInit() || !glfwVulkanSupported())
		return 1;

	Window window;
	window.Init();

	VulkanApp app;
	app.Init("Snake", window.GetWindow());
	while (!glfwWindowShouldClose(window.GetWindow())) {
		app.RenderLoop();
		app.DrawFrame();
		glfwPollEvents();
	}

	glfwTerminate();

	return 0;
}