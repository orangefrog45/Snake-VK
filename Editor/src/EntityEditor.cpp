#include "EntityEditor.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

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

bool EntityEditor::StaticMeshCompEditor(StaticMeshComponent* p_comp) {
	bool ret = false;

	return ret;
}

bool EntityEditor::RenderImGui() {
	bool ret = false;

	if (ImGui::Begin("Entity editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		if (!mp_active_entity)
			goto end_window;
		
		ret |= ImGui::InputText("##tag", &mp_active_entity->GetComponent<TagComponent>()->name);
		ret |= TransformCompEditor(mp_active_entity->GetComponent<TransformComponent>());
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