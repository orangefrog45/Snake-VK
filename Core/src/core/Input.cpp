#include "core/Input.h"
#include <GLFW/glfw3.h>

namespace SNAKE {
	void Input::OnUpdate(struct GLFWwindow* p_window) {
		m_last_mouse_position = m_mouse_position;
		// Press state only valid for one frame, then the key is considered "held"
		for (auto& [key, v] : m_key_states) {
			if (v == InputType::PRESS)
				v = InputType::HELD;
		}

		for (auto& [key, v] : m_mouse_states) {
			if (v == InputType::PRESS)
				v = InputType::HELD;
		}

		double x, y;
		glfwGetCursorPos(p_window, &x, &y);
		m_mouse_position = { x, y };
	}

}