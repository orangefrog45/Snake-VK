#include "EditorLayer.h"
#include "components/Components.h"
#include "rendering/VkRenderer.h"
#include "imgui.h"


using namespace SNAKE;

void CreateLargeEntity(Scene& scene) {
	auto* p_parent = scene.CreateEntity();
	p_parent->GetComponent<TagComponent>()->name = "PARENT";
	for (int i = 0; i < 10; i++) {
		auto* p_child = scene.CreateEntity();
		p_child->SetParent(*p_parent);

		for (int j = 0; j < 1000; j++) {
			auto* p_current = scene.CreateEntity();
			p_current->AddComponent<StaticMeshComponent>();
			p_current->GetComponent<TransformComponent>()->SetPosition(rand() % 300, rand() % 300, rand() % 300);
			p_current->SetParent(*p_child);

		}
	}
}

void EditorLayer::OnInit() {
	renderer.Init(*p_window);

	scene.AddDefaultSystems();
	scene.CreateEntity()->AddComponent<StaticMeshComponent>();

	p_cam_ent = scene.CreateEntity();
	p_cam_ent->AddComponent<CameraComponent>()->MakeActive();

	for (int i = 0; i < 5; i++) {
		CreateLargeEntity(scene);
	}
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


void EditorLayer::OnImGuiRender() {
	ImGui::ShowDemoWindow();

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