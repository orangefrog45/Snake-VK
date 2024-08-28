#include "pch/pch.h"
#include "util/UI.h"

using namespace SNAKE;

bool ImGuiWidgets::Popup(const std::string& name, const std::unordered_map<std::string, std::function<void()>>& options, bool open_condition) {
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

bool ImGuiWidgets::Table(const std::string& name, const TableWidgetData& data) {
	bool ret = false;

	if (ImGui::BeginTable(name.c_str(), 2, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_Borders)) {
		for (auto& [key, func] : data) {
			ImGui::TableNextColumn();
			ImGui::Text(key.c_str()); 
			ImGui::TableNextColumn();
			ret |= func();
		}

		ImGui::EndTable();
	}

	return ret;
}
