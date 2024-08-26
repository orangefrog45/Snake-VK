#include "EntityEditor.h"
#include "imgui.h"
#include "AssetEditor.h"
#include "misc/cpp/imgui_stdlib.h"
#include "util/UI.h"

using namespace SNAKE;

bool EntityEditor::TransformCompEditor(TransformComponent* p_comp) {
	bool ret = false;
	glm::vec3 p = p_comp->GetPosition();
	glm::vec3 s = p_comp->GetScale();
	glm::vec3 r = p_comp->GetOrientation();

	ret |= ImGui::InputFloat3("P", &p.x);
	ret |= ImGui::InputFloat3("R", &r.x);
	ret |= ImGui::InputFloat3("S", &s.x);

	if (ret) {
		p_comp->SetPosition(p);
		p_comp->SetScale(s);
		p_comp->SetOrientation(r);
	}

	return ret;
}

bool EntityEditor::PointlightCompEditor(PointlightComponent* p_comp) {
	bool ret = false;
	static float intensity = 1.f;
	//p_comp->colour /= intensity;
	//TableWidgetData table;
	//table["Colour"] = [&] { return ImGui::DragFloat3("##c", &p_comp->colour[0], 1.f, 0.f, 1.f); };
	//table["Intensity"] = [&] { return ImGui::DragFloat("##i", &intensity, 0.01f, 1.f, 25.f); };
	//table["Attentuation [C|L|E]"] = [&] { return ImGui::DragFloat3("##i", &p_comp->attenuation.constant, 0.01f, 0.f, 1.f); };
	//ret |= ImGuiWidgets::Table("##tbl", table);
	//p_comp->colour *= intensity;

	if (ImGui::BeginTable("##e", 2, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_Borders)) {
		p_comp->colour /= intensity; 

		ImGui::TableNextColumn();
		ImGui::Text("Colour"); 
		ImGui::TableNextColumn();
		ret |= ImGui::DragFloat3("##c", &p_comp->colour[0], 1.f, 0.f, 1.f); 
		ImGui::TableNextColumn();
		ImGui::Text("Intensity"); 
		ImGui::TableNextColumn();
		ret |= ImGui::DragFloat("##i", &intensity, 0.01f, 1.f, 25.f); 
		ImGui::TableNextColumn();
		p_comp->colour *= intensity;

		ImGui::Text("Attentuation [C|L|E]");  ImGui::TableNextColumn();
		ret |= ImGui::DragFloat3("##i", &p_comp->attenuation.constant, 0.01f, 0.f, 1.f);

		ImGui::EndTable();
	}

	return ret;
}

bool EntityEditor::SpotlightCompEditor(SpotlightComponent* p_comp) {
	bool ret = false;
	static float intensity = 1.f;

	if (ImGui::BeginTable("##e", 2, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_Borders)) {
		p_comp->colour /= intensity;

		ImGui::TableNextColumn();

		ImGui::Text("Colour");
		ImGui::TableNextColumn();
		ret |= ImGui::DragFloat3("##c", &p_comp->colour[0], 1.f, 0.f, 1.f);
		ImGui::TableNextColumn();

		ImGui::Text("Intensity");
		ImGui::TableNextColumn();
		ret |= ImGui::DragFloat("##i", &intensity, 0.01f, 1.f, 25.f);
		ImGui::TableNextColumn();
		p_comp->colour *= intensity;

		ImGui::Text("Attentuation [C|L|E]");  ImGui::TableNextColumn();
		ret |= ImGui::DragFloat3("##i", &p_comp->attenuation.constant, 0.01f, 0.f, 1.f);
		ImGui::TableNextColumn();

		ImGui::Text("Aperture");  ImGui::TableNextColumn();
		ret |= ImGui::DragFloat("##aperture", &p_comp->aperture, 0.01f, 0.f, 1.f);

		ImGui::EndTable();
	}

	return ret;
}

bool EntityEditor::StaticMeshCompEditor(StaticMeshComponent* p_comp) {
	bool ret = false;

	ImGui::Text(std::format("Mesh Asset: {}", p_comp->mesh_asset->name).c_str());

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MESH")) {
			auto id = *reinterpret_cast<uint64_t*>(payload->Data);
			p_comp->mesh_asset = AssetManager::GetAsset<StaticMeshAsset>(id);
			ret = true;
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::SeparatorText("Materials");
	if (ImGui::BeginTable("##materials", p_comp->materials.size(), ImGuiTableFlags_Borders)) {
		for (auto& mat : p_comp->materials) {
			ImGui::TableNextColumn();
			ImGui::PushID(&mat);

			AssetEntry entry;
			entry.image = p_asset_editor->GetOrCreateAssetImage(mat.get());
			entry.p_asset = mat.get();
			entry.popup_settings = {};

			p_asset_editor->RenderAssetEntry(entry);

			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL")) {
					auto id = *reinterpret_cast<uint64_t*>(payload->Data);
					mat = AssetManager::GetAsset<MaterialAsset>(id);
					ret = true;
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	return ret;
}

bool EntityEditor::RenderImGui() {
	bool ret = false;

	if (ImGui::Begin("Entity editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		if (!mp_active_entity)
			goto end_window;
		
		ret |= ImGui::InputText("##tag", &mp_active_entity->GetComponent<TagComponent>()->name);
		ret |= TransformCompEditor(mp_active_entity->GetComponent<TransformComponent>());
		if (auto* p_comp = mp_active_entity->GetComponent<StaticMeshComponent>()) ret |= StaticMeshCompEditor(p_comp);
		if (auto* p_comp = mp_active_entity->GetComponent<PointlightComponent>()) ret |= PointlightCompEditor(p_comp);
		if (auto* p_comp = mp_active_entity->GetComponent<SpotlightComponent>()) ret |= SpotlightCompEditor(p_comp);
	}

end_window:
	ImGui::End();
	return ret;
}

void EntityEditor::SelectEntity(Entity* p_ent) {
	mp_active_entity = p_ent;
}

void EntityEditor::DeselectEntity(Entity* p_ent) {
	mp_active_entity = nullptr;
}

void EntityEditor::DestroyEntity(Entity* p_ent) {
	if (mp_active_entity == p_ent)
		DeselectEntity(mp_active_entity);

	p_scene->DeleteEntity(p_ent);
}