#pragma once
#include "VkCommon.h"
#include "core/VkIncl.h"
#include "core/Input.h"
#include "resources/Images.h"


struct GLFWwindow;

namespace SNAKE {
	struct WindowContextVK {
		vk::UniqueSwapchainKHR swapchain;

		std::vector<std::unique_ptr<Image2D>> swapchain_images;

		// Used for ImGui
		std::vector<vk::UniqueImageView> swapchain_linear_image_views;

		vk::Format swapchain_format;
		vk::Extent2D swapchain_extent;
		vk::UniqueSurfaceKHR surface;
	};

	class Window {
	public:
		void Init(const std::string& window_name, uint32_t width, uint32_t height, bool resizable);

		inline uint32_t GetWidth() const { return m_width; };
		inline uint32_t GetHeight() const { return m_height; }

		void CreateSwapchain();
		void RecreateSwapChain();
		void CreateSurface();

		void OnPresentNeedsResize();

		void Shutdown();

		GLFWwindow* GetGLFWwindow() {
			return p_window;
		}

		void OnUpdate();

		const WindowContextVK& GetVkContext() {
			return m_vk_context;
		}

		bool WasJustResized() const {
			return m_just_resized;
		}

		Input input;
	private:
		vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
		vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats);
		vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& available_present_modes);

		unsigned m_width = 0;
		unsigned m_height = 0;

		bool m_just_resized = false;

		GLFWwindow* p_window = nullptr;

		WindowContextVK m_vk_context;

		friend class VulkanApp;
		friend void FramebufferResizeCallback(GLFWwindow* p_window, [[maybe_unused]] int width, [[maybe_unused]] int height);
	};
}