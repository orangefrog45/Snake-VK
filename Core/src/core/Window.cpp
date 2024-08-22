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

		auto* p_win_obj = reinterpret_cast<Window*>(glfwGetWindowUserPointer(p_window));
		p_win_obj->input.KeyCallback(key, scancode, action, mods);
	}

	void GLFW_MouseCallback(GLFWwindow* window, int button, int action, int mods) {
		auto* p_win_obj = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
		p_win_obj->input.MouseCallback(button, action, mods);
	}

	void FramebufferResizeCallback(GLFWwindow* p_window, [[maybe_unused]] int width, [[maybe_unused]] int height) {
		auto* p_win_obj = reinterpret_cast<Window*>(glfwGetWindowUserPointer(p_window));
		p_win_obj->m_just_resized = true;

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

void Window::OnUpdate() {
	input.OnUpdate(p_window);
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
	glfwSetMouseButtonCallback(p_window, GLFW_MouseCallback);

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

void Window::Shutdown() {
	for (auto& image : m_vk_context.swapchain_images) {
		image->DestroyImageView();
		image->DestroySampler();
	}

	m_vk_context.swapchain.release();
	m_vk_context.surface.release();
}

void Window::CreateSwapchain() {
	int width = 0, height = 0;
	glfwGetFramebufferSize(p_window, &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(p_window, &width, &height);
		glfwWaitEvents();
	}

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
	create_info.imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
	create_info.clipped = VK_TRUE; // Don't care about color of pixels that are obscured by other windows
	create_info.oldSwapchain = VK_NULL_HANDLE;

	auto& l_device = VulkanContext::GetLogicalDevice().device;

	m_vk_context.swapchain = l_device->createSwapchainKHRUnique(create_info).value;

	uint32_t swapchain_image_count;
	SNK_CHECK_VK_RESULT(
		l_device->getSwapchainImagesKHR(*m_vk_context.swapchain, &swapchain_image_count, nullptr)
	);

	m_vk_context.swapchain_images.resize(image_count);

	std::vector<vk::Image> temp_images{ swapchain_image_count };

	SNK_CHECK_VK_RESULT(
		l_device->getSwapchainImagesKHR(*m_vk_context.swapchain, &swapchain_image_count, temp_images.data())
	);

	Image2DSpec swapchain_spec;
	swapchain_spec.format = surface_format.format;
	swapchain_spec.size = { extent.width, extent.height };
	swapchain_spec.aspect_flags = vk::ImageAspectFlagBits::eColor;
	swapchain_spec.usage = vk::ImageUsageFlagBits::eColorAttachment;

	for (int i = 0; i < temp_images.size(); i++) {
		m_vk_context.swapchain_images[i] = std::make_unique<Image2D>(temp_images[i], swapchain_spec);
		m_vk_context.swapchain_images[i]->CreateImageView();
	}
	m_vk_context.swapchain_format = surface_format.format;
	m_vk_context.swapchain_extent = extent;

}


void Window::RecreateSwapChain() {
	SNK_CHECK_VK_RESULT(
		VulkanContext::GetLogicalDevice().device->waitIdle()
	);

	for (auto& image : m_vk_context.swapchain_images) {
		image->DestroyImageView();
		image->DestroySampler();
	}

	m_vk_context.swapchain.release();
	m_vk_context.swapchain_images.clear();

	CreateSwapchain();
}

void Window::OnPresentNeedsResize() {
	m_just_resized = false;
	RecreateSwapChain();
}


void Window::CreateSurface() {
	VkSurfaceKHR surface_temp;

	auto res = vk::Result(glfwCreateWindowSurface(*VulkanContext::GetInstance(), p_window, nullptr, &surface_temp));
	m_vk_context.surface = vk::UniqueSurfaceKHR{ surface_temp, *VulkanContext::GetInstance()};
	SNK_CHECK_VK_RESULT(res);
}

