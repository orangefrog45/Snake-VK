#pragma once
#include <imgui.h>

namespace SNAKE {
	using TableWidgetData = std::unordered_map<std::string, std::function<bool()>>;

	struct ImGuiWidgets {
		static bool Popup(const std::string& name, const std::unordered_map<std::string, std::function<void()>>& options, bool open_condition);

		static void Label(const std::string& name) {
			ImGui::Text(name.c_str()); ImGui::SameLine();
		}

		static bool Table(const std::string& name, const TableWidgetData& data);
	};
}