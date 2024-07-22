#pragma once

struct GLFWwindow;

namespace SNAKE {
	class Window {
	public:
		static void InitGLFW();
		void Init(const std::string& window_name, uint32_t width, uint32_t height, bool resizable);

		inline uint32_t GetWidth() const { return m_width; };
		inline uint32_t GetHeight() const { return m_height; }
	private:
		unsigned m_width = 0;
		unsigned m_height = 0;

		GLFWwindow* p_window = nullptr;

		inline static bool s_glfw_initialized = false;

		friend class VulkanApp;
		friend void FramebufferResizeCallback(GLFWwindow* p_window, [[maybe_unused]] int width, [[maybe_unused]] int height);
	};
}