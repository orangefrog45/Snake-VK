#pragma once
#include "VkCommon.h"
#include <core/VkIncl.h>



struct GLFWwindow;

namespace SNAKE {
	class Window {
	public:
		static void InitGLFW();
		void Init(const std::string& window_name, uint32_t width, uint32_t height, bool resizable);

		inline uint32_t GetWidth() const { return m_width; };
		inline uint32_t GetHeight() const { return m_height; }

		void CreateSwapchain();
		void RecreateSwapChain();
		void CreateSurface();
	private:
		void CreateImageViews();

		vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
		vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats);
		vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& available_present_modes);

		unsigned m_width = 0;
		unsigned m_height = 0;

		GLFWwindow* p_window = nullptr;

		struct WindowContextVK {
			vk::UniqueSwapchainKHR swapchain;

			std::vector<vk::Image> swapchain_images;
			std::vector<vk::UniqueImageView> swapchain_image_views;

			vk::Format swapchain_format;
			vk::Extent2D swapchain_extent;
			vk::UniqueSurfaceKHR surface;
		} m_vk_context;

		inline static bool s_glfw_initialized = false;

		friend class VulkanApp;
		friend void FramebufferResizeCallback(GLFWwindow* p_window, [[maybe_unused]] int width, [[maybe_unused]] int height);
	};
}