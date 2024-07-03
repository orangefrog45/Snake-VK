#include <iostream>
#include "GLFW/glfw3.h"
#include <vulkan/vulkan.hpp>
#include "util.h"

class VulkanApp {
public:
	void Init(const char* app_name) {
		
	}

	void CreateInstance() {
		
	}

	VkInstance p_instance = nullptr;
};

void GLFW_KeyCallback(GLFWwindow* p_window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(p_window, GLFW_TRUE);
	}
}

int main() {
	SNAKE::Logger::Init();

	if (!glfwInit() || !glfwVulkanSupported())
		return 1;

	// Default API is OpenGL so set to GLFW_NO_API
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, false);

	GLFWwindow* p_window = glfwCreateWindow(1920, 1080, "ORNG VK", nullptr, nullptr);
	if (!p_window) {
		glfwTerminate();
		SNK_BREAK("Failed to create GLFW window");
	}

	glfwSetKeyCallback(p_window, GLFW_KeyCallback);

	while (!glfwWindowShouldClose(p_window)) {
		glfwPollEvents();
	}

	glfwTerminate();

	return 0;
}