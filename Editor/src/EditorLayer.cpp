#include "EditorLayer.h"
#include "components/Components.h"
#include "rendering/VkRenderer.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "util/FileUtil.h"
#include "scene/SceneSerializer.h"

using namespace SNAKE;

void CreateLargeEntity(Scene& scene) {
	auto& parent = scene.CreateEntity();
	parent.GetComponent<TagComponent>()->name = "PARENT";
	for (int i = 0; i < 10; i++) {
		auto& child = scene.CreateEntity();
		child.SetParent(parent);

		for (int j = 0; j < 10; j++) {
			auto& p_current = scene.CreateEntity();
			p_current.AddComponent<StaticMeshComponent>();
			p_current.GetComponent<TransformComponent>()->SetPosition(rand() % 300, rand() % 300, rand() % 300);
			p_current.SetParent(child);

		}
	}
}


void EditorLayer::CreateProject(const std::string& directory, const std::string& project_name) {
	if (!files::PathExists(directory)) {
		SNK_CORE_ERROR("CreateProject failed, invalid directory");
		return;
	}

	std::string project_dir = directory + "/" + project_name;

	files::Create_Directory(project_dir);
	files::Create_Directory(project_dir + "/res");
	files::Create_Directory(project_dir + "/res/meshes");
	files::Create_Directory(project_dir + "/res/textures");
	files::Create_Directory(project_dir + "/res/materials");
	std::ofstream scene_json(project_dir + "/scene.json");
	scene_json.close();
	std::ofstream project_json(project_dir + "/project.json");
	project_json.close();
}

struct NewProjectData {
	std::string err_msg;
	std::string directory;
	std::string name;
};

void EditorLayer::PromptCreateNewProject() {
	auto& box = *CreateDialogBox();
	box.name = "New project";
	box.data = std::make_shared<NewProjectData>();

	box.imgui_render_cb = [&] {
		if (ImGui::Button("X")) {
			box.close = true;
			return;
		}

		auto project_data = std::any_cast<std::shared_ptr<NewProjectData>>(box.data);
		auto& location_input = project_data->directory;
		auto& name_input = project_data->name;

		ImGui::TextColored({ 1, 0, 0, 1 }, project_data->err_msg.c_str());
		ImGui::Text("Location:");  ImGui::SameLine();  ImGui::InputText("##location", &location_input);
		ImGui::Text("Name:");  ImGui::SameLine();  ImGui::InputText("##name", &name_input);
		if (ImGui::Button("Create")) {
			if (location_input.empty() || name_input.empty()) {
				project_data->err_msg = "Name/Location cannot be empty";
				return;
			}

			if (!std::isalpha(location_input[0]) || !std::isalpha(name_input[0])) {
				project_data->err_msg = "Name/location cannot start with a number";
				return;
			}

			bool valid_input = true;
			std::ranges::for_each(location_input, [&](char c) {if (c != '/' && c != ':' && c != '\\' && !std::isalnum(c)) valid_input = false; });
			std::ranges::for_each(name_input, [&](char c) {if (!std::isalnum(c)) valid_input = false; });
			if (!valid_input) {
				project_data->err_msg = "Name/location cannot contain special characters";
				return;
			}

			CreateProject(location_input, name_input);
			box.close = true;
		}
	};

}

void EditorLayer::LoadProject(const std::string& project_path) {
	std::string project_settings_path = project_path + "/project.json";
	if (!files::PathExists(project_settings_path)) {
		SNK_CORE_ERROR("LoadProject failed, project settings file '{}' not found", project_settings_path);
		return;
	}

	active_project_path = project_path;

	try {
		nlohmann::json j = nlohmann::json::parse(files::ReadTextFile(project_settings_path));
		// Load settings here
	}
	catch (std::exception& e) {
		SNK_CORE_ERROR("Error loading project settings: '{}'", e.what());
		return;
	}

	std::string project_scene_path = project_path + "/scene.json";
	if (!files::PathExists(project_scene_path)) {
		SNK_CORE_ERROR("LoadProject failed, project scene file '{}' not found", project_scene_path);
		return;
	}

	SceneSerializer::DeserializeScene(project_scene_path, scene);
}

void EditorLayer::ToolbarGUI() {
	ImGui::SetNextWindowSize({ (float)p_window->GetWidth(), 50.f });
	ImGui::SetNextWindowPos({ 0, 0 });
	if (ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration)) {
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("Files")) {
				if (ImGui::Button("New project")) {
					PromptCreateNewProject();
				}
				ImGui::EndMenu();
			}
		}
	}

	ImGui::End();
}

void EditorLayer::OnInit() {
	renderer.Init(*p_window);

	scene.AddDefaultSystems();
	scene.CreateEntity().AddComponent<StaticMeshComponent>();

	p_cam_ent = &scene.CreateEntity();
	p_cam_ent->AddComponent<CameraComponent>()->MakeActive();

	//SceneSerializer::DeserializeScene("scene.json", scene);

	auto& box = *CreateDialogBox();
	box.imgui_render_cb = [&] {
		if (ImGui::Button("Close"))
			box.close = true;
	};

	LoadProject("C:\\Users\\Sam\\Documents\\e");
}



void EditorLayer::OnUpdate() {
	glm::vec3 move{ 0,0,0 };
	auto* p_transform = p_cam_ent->GetComponent<TransformComponent>();

	if (p_window->input.IsMouseDown(1)) {
		auto delta = p_window->input.GetMouseDelta();
		glm::vec3 forward_rotate_y = glm::angleAxis(-0.01f * delta.x, glm::vec3{ 0, 1, 0 }) * p_transform->forward;
		glm::vec3 forward_rotate_x = glm::angleAxis(-0.01f * delta.y, p_transform->right) * forward_rotate_y;
		p_transform->LookAt(p_transform->GetPosition() + forward_rotate_x);
	}

	if (p_window->input.IsKeyDown('w'))
		move += p_transform->forward;

	if (p_window->input.IsKeyDown('s'))
		move -= p_transform->forward;

	if (p_window->input.IsKeyDown('d'))
		move += p_transform->right;

	if (p_window->input.IsKeyDown('a'))
		move -= p_transform->right;

	p_transform->SetPosition(p_transform->GetPosition() + move * 0.1f);
}

void EditorLayer::OnRender() {
	uint32_t image_index;
	vk::Semaphore image_avail_semaphore = VkRenderer::AcquireNextSwapchainImage(*p_window, image_index);
	renderer.RenderScene(&scene, *p_window->GetVkContext().swapchain_images[image_index], image_avail_semaphore);
}

struct EntityNode {
	Entity* p_ent = nullptr;
	unsigned tree_depth = 0;
};

void PushEntityIntoHierarchy(Scene* p_scene, Entity* p_ent, std::vector<EntityNode>& entity_hierarchy, std::unordered_set<Entity*>& seen_entity_set) {
	unsigned tree_depth = 0;
	entt::entity current_parent_handle = p_ent->GetParent();
	while (current_parent_handle != entt::null) {
		tree_depth++;
		current_parent_handle = p_scene->GetEntity(current_parent_handle)->GetParent();
	}

	entity_hierarchy.push_back(EntityNode{ .p_ent = p_ent, .tree_depth = tree_depth });
	seen_entity_set.emplace(p_ent);
}

// Arranges entities into linear array, positioned for rendering with ImGui as nodes in the correct order
std::vector<EntityNode> CreateLinearEntityHierarchy(Scene* p_scene) {
	std::vector<EntityNode> entity_hierarchy;
	std::unordered_set<Entity*> seen_entity_set;
	
	for (auto* p_ent : p_scene->GetEntities()) {
		if (seen_entity_set.contains(p_ent))
			continue;

		PushEntityIntoHierarchy(p_scene, p_ent, entity_hierarchy, seen_entity_set);
		p_ent->ForEachChildRecursive([&](entt::entity entt_handle) {
			PushEntityIntoHierarchy(p_scene, p_scene->GetEntity(entt_handle), entity_hierarchy, seen_entity_set);
			});
	}

	return entity_hierarchy;
}

std::string GetEntityPadding(unsigned tree_depth) {
	std::string padding = "";
	for (int i = 0; i < tree_depth; i++) {
		padding += "   ";
	}

	return padding;
}

DialogBox* EditorLayer::CreateDialogBox() {
	return &dialog_boxes.emplace_back();
}

void EditorLayer::RenderDialogBoxes() {
	for (size_t i = 0; i < dialog_boxes.size(); i++) {
		if (dialog_boxes[i].close) {
			dialog_boxes.erase(dialog_boxes.begin() + i);
			i--;
			continue;
		}

		auto& box = dialog_boxes[i];
		ImGui::PushID(&box);


		auto pos = (glm::vec2(p_window->GetWidth(), p_window->GetHeight()) - glm::vec2(box.dimensions)) * 0.5f;
		ImGui::SetNextWindowSize({ (float)box.dimensions.x, (float)box.dimensions.y });
		ImGui::SetNextWindowPos({ pos.x, pos.y });
		bool mouse_over_dialog_box = false;
		if (ImGui::Begin(box.name.c_str())) {
			mouse_over_dialog_box = ImGui::IsMouseHoveringRect({pos.x, pos.y}, {pos.x + box.dimensions.x, pos.y + box.dimensions.y});
			box.imgui_render_cb();
		}

		ImGui::End();

		if (box.block_other_window_input && !mouse_over_dialog_box) {
			ImGui::SetNextWindowPos({ 0, 0 });
			ImGui::SetNextWindowSize({ (float)p_window->GetWidth(), (float)p_window->GetHeight() });
			ImGui::Begin("blocker", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
			ImGui::End();
		}

		ImGui::PopID(); // &box
	}
}

void EditorLayer::OnImGuiRender() {
	ImGui::ShowDemoWindow();
	RenderDialogBoxes();
	ToolbarGUI();

	std::vector<EntityNode> entity_hierarchy = CreateLinearEntityHierarchy(&scene);

	if (ImGui::Begin("Entities")) {
		for (auto entity_node : entity_hierarchy) {
			auto& ent = *entity_node.p_ent;

			// Is entity visible in open nodes
			if (auto parent = ent.GetParent(); parent != entt::null && !open_entity_nodes.contains(scene.GetEntity(parent)))
				continue;

			ImGui::PushID(&ent);
		
			ImGui::Text(std::format("{}{}", GetEntityPadding(entity_node.tree_depth), ent.GetComponent<TagComponent>()->name).c_str());
			if (ImGui::IsItemClicked()) {

				if (open_entity_nodes.contains(&ent))
					open_entity_nodes.erase(&ent);
				else if (ent.HasChildren())
					open_entity_nodes.insert(&ent);

				ent_editor.SelectEntity(&ent);
			}

			ImGui::PopID();
		}
	}

	ent_editor.RenderImGui();

	ImGui::End();
}

void EditorLayer::OnShutdown() {}