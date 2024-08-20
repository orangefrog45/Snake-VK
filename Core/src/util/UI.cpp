#include "pch/pch.h"
#include "util/UI.h"

using namespace SNAKE;

bool ImGuiUtil::Popup(const std::string& name, const std::unordered_map<std::string, std::function<void()>>& options, bool open_condition) {
	if (open_condition) {
		ImGui::OpenPopup(name.c_str());
	}

	bool selected = false;

	if (ImGui::BeginPopup(name.c_str())) {
		for (auto it = options.begin(); it != options.end(); it++) {
			if (ImGui::Selectable(it->first.c_str())) {
				it->second();
				selected = true;
			}
		}
		ImGui::EndPopup();
	}

	return selected;
}
