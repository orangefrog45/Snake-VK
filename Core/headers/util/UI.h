#pragma once
#include <imgui.h>

namespace SNAKE {
	struct ImGuiUtil {
		static bool Popup(const std::string& name, const std::unordered_map<std::string, std::function<void()>>& options, bool open_condition);
	};
}