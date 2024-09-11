#include "EditorLayer.h"
#include "components/Components.h"
#include "rendering/VkRenderer.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "util/FileUtil.h"
#include "scene/SceneSerializer.h"
#include "scene/SceneInfoBufferSystem.h"
#include "util/UI.h"
#include "core/JobSystem.h"
#include "scene/RaytracingBufferSystem.h"

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
			glm::vec3 rand_pos{ rand() % 300 - 150, rand() % 300 - 150, rand() % 300 - 150 };
			rand_pos *= 0.25f;
			p_current.GetComponent<TransformComponent>()->SetPosition(rand_pos);
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
	files::FileCopy(editor_executable_dir + "/res/project-template/", project_dir + "/", true);

	auto* p_box = CreateDialogBox();
	p_box->name = "New project";
	p_box->imgui_render_cb = [this, p_box, project_dir] {
		ImGui::Text("Project created");
		ImGui::Text(std::format("Make active? {}", unsaved_changes ? "There are currently unsaved changes" : "").c_str());

		if (ImGui::Button("Yes")) {
			LoadProject(project_dir);
			p_box->close = true;
		}

		ImGui::SameLine();

		if (ImGui::Button("No"))
			p_box->close = true;
	};
}

struct NewProjectData {
	std::string err_msg;
	std::string directory;
	std::string name;
};

void EditorLayer::PromptCreateNewProject() {
	auto* p_box = CreateDialogBox();
	p_box->name = "New project";
	p_box->data = std::make_shared<NewProjectData>();

	p_box->imgui_render_cb = [this, p_box] {
		if (ImGui::Button("X")) {
			p_box->close = true;
			return;
		}

		auto project_data = std::any_cast<std::shared_ptr<NewProjectData>>(p_box->data);
		auto& location_input = project_data->directory;
		auto& name_input = project_data->name;

		ImGui::TextColored({ 1, 0, 0, 1 }, project_data->err_msg.c_str());
		ImGui::Text("Location:");  ImGui::SameLine();  ImGui::Text(location_input.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Search"))
			location_input = files::SelectDirectoryFromExplorer("");

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
			p_box->close = true;
		}
	};

}

void EditorLayer::SaveProject() {
	nlohmann::json j;
	j["OpenScene"] = scene.name;
	files::WriteTextFile(project.directory + "/project.json", j.dump(4));

	SceneSerializer::SerializeScene(project.directory + std::format("/res/scenes/{}.json", scene.name), scene);
	asset_editor.SerializeAllAssets();
}

void EditorLayer::LoadProject(const std::string& project_path) {
	scene.ClearEntities();
	AssetManager::Clear();

	std::string project_settings_path = project_path + "/project.json";
	if (!files::PathExists(project_settings_path)) {
		SNK_CORE_ERROR("LoadProject failed, project settings file '{}' not found", project_settings_path);
		return;
	}
	
	project.directory = project_path;

	try {
		nlohmann::json j = nlohmann::json::parse(files::ReadTextFile(project_settings_path));
		std::string open_scene_name = j.at("OpenScene").template get<std::string>();
		scene.name = open_scene_name;

		if (open_scene_name.empty()) {
			SNK_CORE_WARN("Project loaded with no scene");
		}

		project.active_scene_path = "";

		for (auto& entry : std::filesystem::directory_iterator(project.directory + "/res/scenes")) {
			if (entry.path().string().ends_with(open_scene_name + ".json")) {
				project.active_scene_path = entry.path().string();
				break;
			}
		}

		if (project.active_scene_path.empty()) {
			SNK_CORE_ERROR("LoadProject failed, tried to open scene '{}' which was not found", open_scene_name);
			return;
		}

		asset_editor.DeserializeAllAssetsFromActiveProject();
		SceneSerializer::DeserializeScene(project.active_scene_path, scene);
	}
	catch (std::exception& e) {
		scene.name = "Unnamed scene";
		SNK_CORE_ERROR("Error loading project settings: '{}'", e.what());
		return;
	}

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
				if (ImGui::Button("Load project")) {
					std::string path = files::SelectFileFromExplorer();
					if (path.empty() || !path.ends_with("project.json")) {
						ErrorMessagePopup("Invalid project directory");
					}
					else {
						LoadProject(files::GetFileDirectory(path));
					}
				}
				if (ImGui::Button("Save project")) {
					SaveProject();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
	}

	ImGui::End();
}

void EditorLayer::InitGBuffer() {
	// Create depth image
	auto depth_format = FindDepthFormat();
	Image2DSpec depth_spec{};
	depth_spec.format = depth_format;
	depth_spec.size = { p_window->GetWidth(), p_window->GetHeight() };
	depth_spec.tiling = vk::ImageTiling::eOptimal;
	depth_spec.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
	depth_spec.aspect_flags = vk::ImageAspectFlagBits::eDepth | (HasStencilComponent(depth_format) ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eNone);
	
	Image2DSpec albedo_spec{};
	albedo_spec.format = vk::Format::eR8G8B8A8Snorm;
	albedo_spec.size = { p_window->GetWidth(), p_window->GetHeight() };
	albedo_spec.tiling = vk::ImageTiling::eOptimal;
	albedo_spec.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
	albedo_spec.aspect_flags = vk::ImageAspectFlagBits::eColor;

	Image2DSpec normal_spec{};
	normal_spec.format = vk::Format::eR16G16B16A16Sfloat;
	normal_spec.size = { p_window->GetWidth(), p_window->GetHeight() };
	normal_spec.tiling = vk::ImageTiling::eOptimal;
	normal_spec.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
	normal_spec.aspect_flags = vk::ImageAspectFlagBits::eColor;

	gbuffer.albedo_image.SetSpec(albedo_spec);
	gbuffer.albedo_image.CreateImage();
	gbuffer.albedo_image.CreateImageView();
	gbuffer.albedo_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, 
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

	gbuffer.normal_image.SetSpec(normal_spec);
	gbuffer.normal_image.CreateImage();
	gbuffer.normal_image.CreateImageView();
	gbuffer.normal_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

	gbuffer.depth_image.SetSpec(depth_spec);
	gbuffer.depth_image.CreateImage();
	gbuffer.depth_image.CreateImageView();
	gbuffer.depth_image.TransitionImageLayout(vk::ImageLayout::eUndefined,
		(HasStencilComponent(depth_format) ? vk::ImageLayout::eDepthAttachmentOptimal : vk::ImageLayout::eDepthAttachmentOptimal), 0);

}

void EditorLayer::OnInit() {
	VkRenderer::QueueDebugRenderLine({ 0, 0, 0 }, { 0, 10000, 0 }, { 0, 1, 0, 1 });

	InitGBuffer();

	Image2DSpec spec;
	spec.aspect_flags = vk::ImageAspectFlagBits::eColor;
	spec.format = vk::Format::eR32G32B32A32Sfloat;
	spec.size = { p_window->GetWidth(), p_window->GetHeight() };
	spec.tiling = vk::ImageTiling::eOptimal;
	spec.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage;

	render_image.SetSpec(spec);
	render_image.CreateImage();
	render_image.CreateImageView();

	for (auto& cmd_buf : m_cmd_buffers) {
		cmd_buf.Init(vk::CommandBufferLevel::ePrimary);
	}

	editor_executable_dir = std::filesystem::current_path().string();
	renderer.Init(&scene);
	ent_editor.Init(&asset_editor);
	asset_editor.Init();

	entity_deletion_listener.callback = [&](Event const* _event) {
		auto* p_casted = dynamic_cast<EntityDeleteEvent const*>(_event);
		ent_editor.DeselectEntity(p_casted->p_ent);
	};
	EventManagerG::RegisterListener<EntityDeleteEvent>(entity_deletion_listener);

	scene.AddDefaultSystems();

	p_cam_ent = std::make_unique<Entity>(&scene, scene.GetRegistry().create(), &scene.GetRegistry(), 0);
	p_cam_ent->AddComponent<TransformComponent>();
	p_cam_ent->AddComponent<RelationshipComponent>();
	p_cam_ent->AddComponent<CameraComponent>()->MakeActive();

	LoadProject("res/project-template");

	for (int i = 0; i < 10; i++) {
	//	CreateLargeEntity(scene);
	}
	raytracing.Init(scene, render_image, *scene.GetSystem<SceneInfoBufferSystem>()->descriptor_buffers[0].GetDescriptorSpec());

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

	float speed = 0.1f;
	if (p_window->input.IsKeyDown(Key::LeftControl))
		speed *= 0.1f;

	if (p_window->input.IsKeyDown(Key::Shift))
		speed *= 10.f;

	p_transform->SetPosition(p_transform->GetPosition() + move * speed);
}

void EditorLayer::OnRender() {
	uint32_t image_index;
	vk::Semaphore image_avail_semaphore = VkRenderer::AcquireNextSwapchainImage(*p_window, image_index);
	auto& swapchain_image = p_window->GetVkContext().swapchain_images[image_index];

	raytracing.RecordRenderCmdBuf(*m_cmd_buffers[VkContext::GetCurrentFIF()].buf, render_image, scene.GetSystem<SceneInfoBufferSystem>()->descriptor_buffers[VkContext::GetCurrentFIF()]);
	//renderer.RenderScene(render_image, depth_image);
	//m_cmd_buffers[VkContext::GetCurrentFIF()].buf->reset();
	//VkRenderer::RecordRenderDebugCommands(*m_cmd_buffers[VkContext::GetCurrentFIF()].buf, render_image, depth_image, 
	//	scene.GetSystem<SceneInfoBufferSystem>()->descriptor_buffers[VkContext::GetCurrentFIF()]);

	vk::SubmitInfo submit_info{};
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &*m_cmd_buffers[VkContext::GetCurrentFIF()].buf;
	VkContext::GetLogicalDevice().SubmitGraphics(submit_info);

	render_image.BlitTo(*swapchain_image, 0, 0, vk::ImageLayout::eGeneral, vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eColorAttachmentOptimal, vk::Filter::eNearest, image_avail_semaphore);
	
	asset_editor.Render();
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
	for (unsigned i = 0; i < tree_depth; i++) {
		padding += "   ";
	}

	return padding;
}

DialogBox* EditorLayer::CreateDialogBox() {
	new_dialog_boxes_to_sort.push_back(std::make_unique<DialogBox>());
	return &*new_dialog_boxes_to_sort[new_dialog_boxes_to_sort.size() - 1];
}

void EditorLayer::ErrorMessagePopup(const std::string& err) {
	auto* p_box = CreateDialogBox();
	p_box->block_other_window_input = true;
	p_box->imgui_render_cb = [err, p_box] {
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 25);
		if (ImGui::Button("X")) {
			p_box->close = true;
		}
		ImGui::PushStyleColor(ImGuiCol_Text, { 1, 0, 0, 1 });
		ImGui::SeparatorText("Error");
		ImGui::Text(err.c_str());
		ImGui::PopStyleColor();
	};
}

void EditorLayer::RenderDialogBoxes() {
	// Dialog boxes sorted like this in case a dialog box creates another dialog box in its imgui callback (causes issues in loop)
	for (auto& p_box : new_dialog_boxes_to_sort) {
		dialog_boxes.push_back(std::move(p_box));
	}
	new_dialog_boxes_to_sort.clear();

	for (size_t i = 0; i < dialog_boxes.size(); i++) {
		if (dialog_boxes[i]->close) {
			dialog_boxes.erase(dialog_boxes.begin() + i);
			i--;
			continue;
		}

		auto& box = *dialog_boxes[i];
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

bool EditorLayer::ModifyEntityPopup(bool open_condition, Entity* p_ent) {
	return ImGuiWidgets::Popup("Modify entity",
		{
			{"Delete", [&] {scene.DeleteEntity(p_ent); }}
		}, open_condition
	);
}

bool EditorLayer::CreateEntityPopup(bool open_condition) {
	return ImGuiWidgets::Popup("Create entity",
		{
			{"Empty", [&] {scene.CreateEntity(); }},
			{"Mesh", [&] {scene.CreateEntity().AddComponent<StaticMeshComponent>(); }},
			{"Pointlight", [&] {scene.CreateEntity().AddComponent<PointlightComponent>(); }},
			{"Spotlight", [&] {scene.CreateEntity().AddComponent<SpotlightComponent>()->GetEntity()->AddComponent<PointlightComponent>(); }},
		}, open_condition
	);
}

void EditorLayer::OnImGuiRender() {
	ImGui::ShowDemoWindow();
	RenderDialogBoxes();
	ToolbarGUI();

	unsaved_changes |= asset_editor.RenderImGui();

	std::vector<EntityNode> entity_hierarchy = CreateLinearEntityHierarchy(&scene);

	if (ImGui::Begin("Entities")) {
		ImGui::Text(std::to_string(ImGui::GetIO().Framerate).c_str());
		Entity* p_right_clicked_entity = nullptr;

		for (auto entity_node : entity_hierarchy) {
			auto& ent = *entity_node.p_ent;

			// Is entity visible in open nodes
			if (auto parent = ent.GetParent(); parent != entt::null && !open_entity_nodes.contains(scene.GetEntity(parent)))
				continue;

			ImGui::PushID(&ent);
		
			ImGui::Selectable(std::format("{}{}", GetEntityPadding(entity_node.tree_depth), ent.GetComponent<TagComponent>()->name).c_str(), ent_editor.GetSelectedEntity() == &ent);
			if (ImGui::IsItemClicked(1))
				p_right_clicked_entity = entity_node.p_ent;

			if (ImGui::IsItemClicked()) {
				if (open_entity_nodes.contains(&ent))
					open_entity_nodes.erase(&ent);
				else if (ent.HasChildren())
					open_entity_nodes.insert(&ent);

				ent_editor.SelectEntity(&ent);
			}

			ImGui::PopID();
		}

		unsaved_changes |= CreateEntityPopup(!p_right_clicked_entity && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1));
	}


	unsaved_changes |= ent_editor.RenderImGui();

	ImGui::End();
}

void EditorLayer::OnShutdown() {}