#include "core/Window.h"
#include "util/util.h"

#include <GLFW/glfw3.h>
#include "events/EventsCommon.h"

namespace SNAKE {
	void GLFW_KeyCallback(GLFWwindow* p_window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
			glfwSetWindowShouldClose(p_window, GLFW_TRUE);
		}
	}

	void FramebufferResizeCallback(GLFWwindow* p_window, [[maybe_unused]] int width, [[maybe_unused]] int height) {
		auto* p_win_obj = reinterpret_cast<Window*>(glfwGetWindowUserPointer(p_window));
		//p_app->m_framebuffer_resized = true;

		WindowEvent _event{ .prev_width = p_win_obj->m_width, .prev_height = p_win_obj->m_height, .new_width = (uint32_t)width, .new_height = (uint32_t)height};
		p_win_obj->m_width = width;
		p_win_obj->m_height = height;

		EventManagerG::DispatchEvent(_event);
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

		glfwSetKeyCallback(p_window, GLFW_KeyCallback);
		glfwSetFramebufferSizeCallback(p_window, FramebufferResizeCallback);
	}
}