#include <iostream>
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "util.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

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
		SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(m_physical_device);

		vk::SurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(swap_chain_support.formats);
		vk::PresentModeKHR present_mode = ChooseSwapPresentMode(swap_chain_support.present_modes);
		vk::Extent2D extent = ChooseSwapExtent(swap_chain_support.capabilities, p_window);

		uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1; // Add extra image to prevent having to wait on driver before acquiring another image

		// 0 means no maximum
		if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount)
			image_count = swap_chain_support.capabilities.maxImageCount;

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

		create_info.preTransform = swap_chain_support.capabilities.currentTransform;
		create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque; // Don't blend with other windows in window system
		create_info.presentMode = present_mode;
		create_info.clipped = VK_TRUE; // Don't care about color of pixels that are obscured by other windows
		create_info.oldSwapchain = VK_NULL_HANDLE;

		m_swap_chain = m_device->createSwapchainKHRUnique(create_info);
	}

	bool IsDeviceSuitable(vk::PhysicalDevice device) {
		vk::PhysicalDeviceProperties properties;
		device.getProperties(&properties, VULKAN_HPP_DEFAULT_DISPATCHER);

		if (!CheckDeviceExtensionSupport(device))
			return false;

		SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(device);
		bool swap_chain_valid = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();

		QueueFamilyIndices qf_indices = FindQueueFamilies(device);
		return properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu && qf_indices.IsComplete() && swap_chain_valid;
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

	~VulkanApp() {
	}

	vk::UniqueInstance m_instance;
	vk::UniqueDebugUtilsMessengerEXT m_messenger;
	vk::UniqueSurfaceKHR m_surface;
	vk::PhysicalDevice m_physical_device;
	vk::UniqueDevice m_device;
	vk::Queue m_graphics_queue;
	vk::Queue m_presentation_queue;
	vk::UniqueSwapchainKHR m_swap_chain;

	static constexpr std::array<const char*, 1> m_required_device_extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

};


int main() {
	SNAKE::Logger::Init();

	if (!glfwInit() || !glfwVulkanSupported())
		return 1;

	Window window;
	window.Init();

	VulkanApp app;
	app.Init("Snake", window.GetWindow());
	while (!glfwWindowShouldClose(window.GetWindow())) {
		glfwPollEvents();
	}

	glfwTerminate();

	return 0;
}