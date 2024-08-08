#include "core/VkCommon.h"
#include "core/VkContext.h"
#include "core/Window.h"
#include "events/EventsCommon.h"
#include "util/util.h"

#include <GLFW/glfw3.h>

using namespace SNAKE;

namespace SNAKE {
	void GLFW_KeyCallback(GLFWwindow* p_window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
			glfwSetWindowShouldClose(p_window, GLFW_TRUE);
		}
	}

	void FramebufferResizeCallback(GLFWwindow* p_window, [[maybe_unused]] int width, [[maybe_unused]] int height) {
		auto* p_win_obj = reinterpret_cast<Window*>(glfwGetWindowUserPointer(p_window));
		//p_app->m_framebuffer_resized = true;

		WindowEvent _event;
		_event.prev_width = p_win_obj->m_width;
		_event.prev_height = p_win_obj->m_height;
		_event.new_width = (uint32_t)width;
		_event.new_height = (uint32_t)height;
		p_win_obj->m_width = width;
		p_win_obj->m_height = height;

		EventManagerG::DispatchEvent(_event);
	}
}

void Window::InitGLFW() {
	if (!glfwInit() || !glfwVulkanSupported())
		SNK_BREAK("GLFW failed to initialize");

	s_glfw_initialized = true;
}

void Window::Init(const std::string& window_name, uint32_t width, uint32_t height, bool resizable) {
	// Default API is OpenGL so set to GLFW_NO_API
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, resizable);

	p_window = glfwCreateWindow(width, height, window_name.c_str(), nullptr, nullptr);
	glfwSetWindowUserPointer(p_window, this);

	if (!p_window) {
		glfwTerminate();
		SNK_BREAK("Failed to create GLFW window");
	}

	m_width = width;
	m_height = height;

	glfwSetKeyCallback(p_window, GLFW_KeyCallback);
	glfwSetFramebufferSizeCallback(p_window, FramebufferResizeCallback);
}


// Choose format swap chain images use
vk::SurfaceFormatKHR Window::ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats) {
	for (const auto& format : available_formats) {
		if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			return format;
	}

	// Pick first available as default if ideal format not found
	return available_formats[0];
}

// Choose way in which swap chain images are presented
vk::PresentModeKHR Window::ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& available_present_modes) {
	for (const auto& mode : available_present_modes) {
		if (mode == vk::PresentModeKHR::eMailbox)
			return mode;
	}

	// FIFO guaranteed to be available so pick this as default
	return vk::PresentModeKHR::eFifo;
}

// Choose resolution of swap chain images
vk::Extent2D Window::ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}
	else { // Width/height can differ from capabilities.currentExtent

		vk::Extent2D actual_extent = {
			m_width,
			m_height
		};
		actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actual_extent;
	}
}

void Window::CreateSwapchain() {
	SwapChainSupportDetails swapchain_support = QuerySwapChainSupport(VulkanContext::GetPhysicalDevice().device, *m_vk_context.surface);

	vk::SurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(swapchain_support.formats);
	vk::PresentModeKHR present_mode = ChooseSwapPresentMode(swapchain_support.present_modes);
	vk::Extent2D extent = ChooseSwapExtent(swapchain_support.capabilities);

	uint32_t image_count = swapchain_support.capabilities.minImageCount + 1; // Add extra image to prevent having to wait on driver before acquiring another image

	// 0 means no maximum
	if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount)
		image_count = swapchain_support.capabilities.maxImageCount;

	vk::SwapchainCreateInfoKHR create_info{ vk::SwapchainCreateFlagsKHR(0),
		*m_vk_context.surface,
		image_count,
		surface_format.format,
		surface_format.colorSpace,
		extent,
		1,
		vk::ImageUsageFlagBits::eColorAttachment
	};

	QueueFamilyIndices indices = FindQueueFamilies(VulkanContext::GetPhysicalDevice().device, *m_vk_context.surface);
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

	auto& l_device = VulkanContext::GetLogicalDevice().device;

	m_vk_context.swapchain = l_device->createSwapchainKHRUnique(create_info).value;

	uint32_t swapchain_image_count;
	SNK_CHECK_VK_RESULT(
		l_device->getSwapchainImagesKHR(*m_vk_context.swapchain, &swapchain_image_count, nullptr)
	)

	m_vk_context.swapchain_images.resize(image_count);

	SNK_CHECK_VK_RESULT(
		l_device->getSwapchainImagesKHR(*m_vk_context.swapchain, &swapchain_image_count, m_vk_context.swapchain_images.data())
	)

	m_vk_context.swapchain_format = surface_format.format;
	m_vk_context.swapchain_extent = extent;

	CreateImageViews();
}

void Window::CreateImageViews() {
	m_vk_context.swapchain_image_views.resize(m_vk_context.swapchain_images.size());

	for (size_t i = 0; i < m_vk_context.swapchain_images.size(); i++) {
		vk::ImageViewCreateInfo create_info{ vk::ImageViewCreateFlags{0}, m_vk_context.swapchain_images[i], vk::ImageViewType::e2D, m_vk_context.swapchain_format };
		create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;

		m_vk_context.swapchain_image_views[i] = VulkanContext::GetLogicalDevice().device->createImageViewUnique(create_info).value;

		SNK_ASSERT(m_vk_context.swapchain_image_views[i], "Swapchain image view {0} created", i);
	}
}

void Window::RecreateSwapChain() {
	SNK_CHECK_VK_RESULT(
		VulkanContext::GetLogicalDevice().device->waitIdle()
	);

	m_vk_context.swapchain.reset();
	m_vk_context.swapchain_images.clear();
	m_vk_context.swapchain_image_views.clear();
	

	CreateSwapchain();
}

void Window::CreateSurface() {
	VkSurfaceKHR surface_temp;

	auto res = vk::Result(glfwCreateWindowSurface(*VulkanContext::GetInstance(), p_window, nullptr, &surface_temp));
	m_vk_context.surface = vk::UniqueSurfaceKHR{ surface_temp };
	SNK_CHECK_VK_RESULT(res);
}
