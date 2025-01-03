#include "components/Components.h"
#include "EditorLayer.h"
#include "events/EventsCommon.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "rendering/StreamlineWrapper.h"
#include "rendering/VkRenderer.h"
#include "scene/SceneSerializer.h"
#include "scene/SceneInfoBufferSystem.h"
#include "scene/CameraSystem.h"
#include "scene/ParticleSystem.h"
#include "scene/TlasSystem.h"
#include "util/FileUtil.h"
#include "util/UI.h"

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

void EditorLayer::PhysicsParamsUI() {
	auto& particle_system = *scene.GetSystem<ParticleSystem>();
	PhysicsParamsUBO& params_ubo = particle_system.parameters;

	if (ImGui::Button("Restart sim"))
		particle_system.InitializeSimulation();

	static int num_particles = particle_system.GetNumParticles();
	if (ImGui::InputInt("Num particles", &num_particles)) {
		particle_system.SetNumParticles(num_particles);
	}

	ImGui::DragFloat("Particle restitution", &params_ubo.particle_restitution, 0.01f);
	//ImGui::DragFloat("Timestep", &params_ubo.particle_restitution, 0.01f);
	ImGui::DragFloat("Friction coefficient", &params_ubo.friction_coefficient, 0.01f);
	ImGui::DragFloat("Repulsion factor", &params_ubo.repulsion_factor, 0.01f);
	ImGui::DragFloat("Penetration slop", &params_ubo.penetration_slop, 0.01f);
	ImGui::DragFloat("Restitution slop", &params_ubo.restitution_slop, 0.01f);
	ImGui::DragFloat("Sleep threshold", &params_ubo.sleep_threshold, 0.01f);
}


void EditorLayer::InitGBuffer(glm::vec2 internal_render_dim) {
	// Create depth image
	auto depth_format = FindDepthFormat();
	Image2DSpec depth_spec{};
	depth_spec.format = depth_format;
	depth_spec.size = internal_render_dim;
	depth_spec.tiling = vk::ImageTiling::eOptimal;
	depth_spec.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
	depth_spec.aspect_flags = vk::ImageAspectFlagBits::eDepth | (HasStencilComponent(depth_format) ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eNone);

	Image2DSpec albedo_spec{};
	albedo_spec.format = vk::Format::eR8G8B8A8Snorm;
	albedo_spec.size = internal_render_dim;
	albedo_spec.tiling = vk::ImageTiling::eOptimal;
	albedo_spec.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
	albedo_spec.aspect_flags = vk::ImageAspectFlagBits::eColor;

	Image2DSpec normal_spec = albedo_spec;
	normal_spec.format = vk::Format::eR16G16B16A16Sfloat;
	normal_spec.usage |= vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;

	Image2DSpec rma_spec = normal_spec;
	rma_spec.format = vk::Format::eR16G16B16A16Sfloat;

	Image2DSpec pixel_motion_spec = rma_spec;
	pixel_motion_spec.format = vk::Format::eR16G16Sfloat;
	pixel_motion_spec.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc;

	Image2DSpec mat_flag_spec = rma_spec;
	mat_flag_spec.format = vk::Format::eR16Uint;

	render_resources.albedo_image.SetSpec(albedo_spec);
	render_resources.albedo_image.CreateImage();
	render_resources.albedo_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, 
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

	render_resources.normal_image.SetSpec(normal_spec);
	render_resources.normal_image.CreateImage();
	render_resources.normal_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

	render_resources.prev_frame_normal_image.SetSpec(normal_spec);
	render_resources.prev_frame_normal_image.CreateImage();
	render_resources.prev_frame_normal_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

	render_resources.depth_image.SetSpec(depth_spec);
	render_resources.depth_image.CreateImage();
	render_resources.depth_image.TransitionImageLayout(vk::ImageLayout::eUndefined,
		(HasStencilComponent(depth_format) ? vk::ImageLayout::eDepthAttachmentOptimal : vk::ImageLayout::eDepthAttachmentOptimal), 0);

	render_resources.prev_frame_depth_image.SetSpec(depth_spec);
	render_resources.prev_frame_depth_image.CreateImage();
	render_resources.prev_frame_depth_image.TransitionImageLayout(vk::ImageLayout::eUndefined,
		(HasStencilComponent(depth_format) ? vk::ImageLayout::eShaderReadOnlyOptimal : vk::ImageLayout::eShaderReadOnlyOptimal),
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

	render_resources.rma_image.SetSpec(rma_spec);
	render_resources.rma_image.CreateImage();
	render_resources.rma_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

	render_resources.pixel_motion_image.SetSpec(pixel_motion_spec);
	render_resources.pixel_motion_image.CreateImage();
	render_resources.pixel_motion_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

	render_resources.mat_flag_image.SetSpec(mat_flag_spec);
	render_resources.mat_flag_image.CreateImage();
	render_resources.mat_flag_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

	constexpr uint32_t PIXEL_RESERVOIR_DATA_SIZE = sizeof(float) * 12;
	// 2x size as results are written to different part of the buffer
	render_resources.reservoir_buffer.CreateBuffer(internal_render_dim.x * internal_render_dim.y * PIXEL_RESERVOIR_DATA_SIZE * 2,
		vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer);
}

void EditorLayer::OnInit() {
	m_window_resize_listener.callback = [this]([[maybe_unused]] Event const*) {
		m_render_settings.realloc_render_resources = true;
	};

	EventManagerG::RegisterListener<SwapchainInvalidateEvent>(m_window_resize_listener);

	vk::SemaphoreCreateInfo semaphore_info{};
	auto [semaphore_res, semaphore] = VkContext::GetLogicalDevice().device->createSemaphoreUnique(semaphore_info);
	SNK_CHECK_VK_RESULT(semaphore_res);
	m_compute_graphics_semaphore = std::move(semaphore);

	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_gbuffer_cmd_buffers[i].Init(vk::CommandBufferLevel::ePrimary);
		m_rt_cmd_buffers[i].Init(vk::CommandBufferLevel::ePrimary);
		m_final_blit_cmd_buffers[i].Init(vk::CommandBufferLevel::ePrimary);
	}

	editor_executable_dir = std::filesystem::current_path().string();
	ent_editor.Init(&asset_editor);
	asset_editor.Init();

	entity_deletion_listener.callback = [&](Event const* _event) {
		auto* p_casted = dynamic_cast<EntityDeleteEvent const*>(_event);
		ent_editor.DeselectEntity(p_casted->p_ent);
	};
	EventManagerG::RegisterListener<EntityDeleteEvent>(entity_deletion_listener);

	scene.AddDefaultSystems();
	scene.AddSystem<TlasSystem>();
	scene.AddSystem<ParticleSystem>();

	p_cam_ent = std::make_unique<Entity>(&scene, scene.GetRegistry().create(), &scene.GetRegistry(), 0);
	p_cam_ent->AddComponent<TransformComponent>();
	p_cam_ent->AddComponent<RelationshipComponent>();
	p_cam_ent->AddComponent<CameraComponent>()->MakeActive();

	LoadProject("res/project-template");

	InitRenderResources();
	gbuffer_pass.Init(scene, render_resources);

	raytracing.Init(scene, internal_render_image, scene.GetSystem<SceneInfoBufferSystem>()->descriptor_buffers[0].GetDescriptorSpec(), render_resources);
	m_taa_resolve_pass.Init();
}

void EditorLayer::InitRenderResources() {
	SNK_CHECK_VK_RESULT(VkContext::GetLogicalDevice().device->waitIdle());

	if (bool resources_already_created = internal_render_image.ImageIsCreated()) {
		internal_render_image.DestroyImage();
		display_render_image.DestroyImage();
		render_resources.depth_image.DestroyImage();
		render_resources.albedo_image.DestroyImage();
		render_resources.rma_image.DestroyImage();
		render_resources.pixel_motion_image.DestroyImage();
		render_resources.normal_image.DestroyImage();
		render_resources.prev_frame_normal_image.DestroyImage();
		render_resources.prev_frame_depth_image.DestroyImage();
		render_resources.mat_flag_image.DestroyImage();
		render_resources.reservoir_buffer.DestroyBuffer();
	}

	m_render_settings.internal_render_dim = { p_window->GetWidth(), p_window->GetHeight() };
	if (m_render_settings.dlss_preset != 0) {
		sl::DLSSOptions dlss_options{};
		dlss_options.mode = static_cast<sl::DLSSMode>(m_render_settings.dlss_preset);
		dlss_options.outputWidth = p_window->GetWidth();
		dlss_options.outputHeight = p_window->GetHeight();
		dlss_options.qualityPreset = (sl::DLSSPreset)m_render_settings.dlss_quality_preset;
		dlss_options.ultraPerformancePreset = sl::DLSSPreset::eDefault;
		dlss_options.ultraQualityPreset = sl::DLSSPreset::eDefault;
		dlss_options.performancePreset = sl::DLSSPreset::eDefault;
		dlss_options.balancedPreset = sl::DLSSPreset::eDefault;
		dlss_options.colorBuffersHDR = sl::Boolean::eTrue;
		dlss_options.useAutoExposure = sl::Boolean::eFalse;
		dlss_options.alphaUpscalingEnabled = sl::Boolean::eFalse;
		sl::DLSSOptimalSettings optimal_settings = StreamlineWrapper::GetDLSS_OptimalSettings(dlss_options);
		dlss_options.sharpness = optimal_settings.optimalSharpness;

		m_render_settings.internal_render_dim = { optimal_settings.optimalRenderWidth, optimal_settings.optimalRenderHeight };
		m_streamline.SetDLSS_Options(dlss_options);
	}

	Image2DSpec internal_render_spec;
	internal_render_spec.aspect_flags = vk::ImageAspectFlagBits::eColor;
	internal_render_spec.format = vk::Format::eR16G16B16A16Sfloat;
	internal_render_spec.size = m_render_settings.internal_render_dim;
	internal_render_spec.tiling = vk::ImageTiling::eOptimal;
	internal_render_spec.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc |
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;

	// Only create display render image if DLSS is being used (as the output), if not then internal_render_image will just be used for the full pipeline
	if (m_render_settings.dlss_preset != 0) {
		Image2DSpec display_render_spec = internal_render_spec;
		display_render_spec.size = glm::vec2{ p_window->GetWidth(), p_window->GetHeight() };
		display_render_image.SetSpec(display_render_spec);
		display_render_image.CreateImage();
		display_render_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::AccessFlagBits::eNone,
			vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);
	}

	internal_render_image.SetSpec(internal_render_spec);
	internal_render_image.CreateImage();

	InitGBuffer(m_render_settings.internal_render_dim);
}



void EditorLayer::OnFrameStart() {
	sl::Constants c;

	if (m_render_settings.realloc_render_resources) {
		InitRenderResources();
		m_render_settings.realloc_render_resources = false;
		c.reset = sl::Boolean::eTrue;
		scene.GetSystem<SceneInfoBufferSystem>()->MarkPreviousFrameAsInvalid();
	}
	else {
		c.reset = sl::Boolean::eFalse;
	}

	if (m_render_settings.dlss_preset == 0)
		return;

	auto* p_cam = scene.GetSystem<CameraSystem>()->GetActiveCam();
	auto* p_cam_transform = p_cam->GetEntity()->GetComponent<TransformComponent>();

	auto cam_pos = p_cam_transform->GetAbsPosition();
	memcpy(&c.cameraPos, &cam_pos, sizeof(glm::vec3));
	memcpy(&c.cameraRight, &p_cam_transform->right, sizeof(glm::vec3));
	memcpy(&c.cameraFwd, &p_cam_transform->forward, sizeof(glm::vec3));
	c.cameraUp = { 0, 1, 0 };
	c.cameraNear = p_cam->z_near;
	c.cameraFar = p_cam->z_far;
	c.cameraFOV = glm::radians(p_cam->fov);
	c.cameraAspectRatio = p_cam->aspect_ratio;
	c.cameraPinholeOffset = { 0, 0 };
	
	// Jitter in pixel space
	glm::vec2 jitter = GetFrameJitter(VkContext::GetCurrentFrameIdx(), m_render_settings.internal_render_dim.x, p_window->GetWidth());
	c.jitterOffset = { jitter.x, jitter.y };
	c.mvecScale = { 1, 1 };

	auto proj_matrix = p_cam->GetProjectionMatrix();
	auto view = glm::lookAt(p_cam_transform->GetAbsPosition(), p_cam_transform->GetAbsPosition() + p_cam_transform->forward, glm::vec3{ 0.f, 1.f, 0.f });

	// Streamline expects d3d clip space
	glm::mat4 proj_matrix_d3d = proj_matrix;
	glm::mat4 prev_proj_matrix_d3d = m_prev_frame_projection_matrix;
	proj_matrix_d3d[1][1] *= -1.f;
	prev_proj_matrix_d3d[1][1] *= -1.f;

	glm::mat4 reprojection_matrix = prev_proj_matrix_d3d * (m_prev_frame_view_matrix * glm::inverse(view)) * glm::inverse(proj_matrix_d3d);
	c.cameraViewToClip = StreamlineWrapper::GlmToSl(proj_matrix);
	c.clipToCameraView = StreamlineWrapper::GlmToSl(glm::inverse(proj_matrix));
	c.clipToLensClip = StreamlineWrapper::GlmToSl(glm::identity<glm::mat4>());
	c.clipToPrevClip = StreamlineWrapper::GlmToSl(reprojection_matrix);
	c.prevClipToClip = StreamlineWrapper::GlmToSl(glm::inverse(reprojection_matrix));

	m_prev_frame_projection_matrix = proj_matrix;
	m_prev_frame_view_matrix = view;

	c.depthInverted = sl::Boolean::eFalse;
	c.cameraMotionIncluded = sl::Boolean::eTrue;
	c.motionVectors3D = sl::Boolean::eFalse;
	c.orthographicProjection = sl::Boolean::eFalse;
	c.motionVectorsJittered = sl::Boolean::eFalse;

	if (SL_FAILED(res, slSetConstants(c, *StreamlineWrapper::GetCurrentFrameToken(), {}))) {
		SNK_CORE_ERROR("slSetConstants failed, err: {}", (uint32_t)res);
	}
}

void EditorLayer::OnUpdate() {
	auto* p_cam_transform = p_cam_ent->GetComponent<TransformComponent>();

	if (p_window->input.IsKeyPressed('f')) {

		for (int i = 0; i < 200; i++) {
			auto& pillar = scene.CreateEntity();
			pillar.AddComponent<StaticMeshComponent>()->SetMeshAsset(AssetManager::GetAsset<StaticMeshAsset>(AssetManager::CoreAssetIDs::CUBE_MESH));
			pillar.GetComponent<TransformComponent>()->SetPosition(rand() % 1000 - 500.f, 0, rand() % 1000 - 500.f);
			pillar.GetComponent<TransformComponent>()->SetOrientation(rand() % 10, rand() % 10, rand() % 10);
			pillar.GetComponent<TransformComponent>()->SetScale(4.f, rand() % 20 + 100, 4.f);
		}
	}

	auto& entities = scene.GetEntities();
	if (p_window->input.IsKeyPressed('g')) {
		for (int i = 0; i < entities.size(); i++) {
			if (entities[i]->GetComponent<TransformComponent>()->GetScale().y > 99.f) {
				scene.DeleteEntity(entities[i]);
				i--;
			}
		}
	}

	static float h = 0.f;

	auto cols = util::array(glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));
	if (p_window->input.IsKeyPressed('h')) {
		for (float x = 0.f; x <= 0.5f; x++) {
			for (float z = 0.f; z <= 100.f; z++) {
				auto& ent = scene.CreateEntity();
				auto* p_mesh = ent.AddComponent<StaticMeshComponent>();
				p_mesh->SetMeshAsset(AssetManager::GetAsset<StaticMeshAsset>(AssetManager::CoreAssetIDs::CUBE_MESH));
				auto mat = AssetManager::CreateAsset<MaterialAsset>();
				mat->RaiseFlag(MaterialAsset::MaterialFlagBits::EMISSIVE);
				mat->albedo = glm::vec3{ cols[rand() % 3]} * 20.f;
				p_mesh->SetMaterial(0, mat);
				//auto* p_light = ent.AddComponent<PointlightComponent>();
				//p_light->attenuation.exp = 0.1f;
				//p_light->colour = glm::vec3{ abs(sinf(x * 0.1f * 2.f * glm::pi<float>())), (x + z) / 20.f , abs(cosf(z * 0.1f * 2.f * glm::pi<float>()))} * 3.f;
				ent.GetComponent<TransformComponent>()->SetPosition(45.f + (rand() % 50 - 25) * 0.75f, (rand() % 100) * 0.1f + 0.5f, (rand() % 50 - 25) * 0.1f);
				//ent.GetComponent<TransformComponent>()->SetOrientation(rand() % 180, rand() % 180, rand() % 180);
				ent.GetComponent<TransformComponent>()->SetScale(0.1, 0.1, 0.1);
			}
		}
		h += 2.f;
	}

	static float e = 0.f;
	e += 0.5f;

	if (auto* p_ent = scene.GetEntity("Spinner")) {
		auto* p_transform0 = p_ent->GetComponent<TransformComponent>();
		p_transform0->SetOrientation(0.f, e*1.f, 0.f);
		//p_transform0->SetPosition(sin(e * 0.01) * 20.f, 0.f, 0.f);
	}

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

	if (p_window->input.IsKeyDown(Key::Space))
		speed *= 100.f;

	p_transform->SetPosition(p_transform->GetPosition() + move * speed);

}


void EditorLayer::OnRender() {
	FrameInFlightIndex fif = VkContext::GetCurrentFIF();
	auto graphics_cmd_buffers = util::array(*m_gbuffer_cmd_buffers[fif].buf);

	vk::SubmitInfo submit_info_graphics{};
	submit_info_graphics.commandBufferCount = (uint32_t)graphics_cmd_buffers.size();
	submit_info_graphics.pCommandBuffers = graphics_cmd_buffers.data();

	uint32_t image_index;
	vk::Semaphore image_avail_semaphore = VkRenderer::AcquireNextSwapchainImage(*p_window, image_index);
	auto& swapchain_image = p_window->GetVkContext().swapchain_images[image_index];

	m_gbuffer_cmd_buffers[fif].buf->reset();
	m_rt_cmd_buffers[fif].buf->reset();
	vk::CommandBufferBeginInfo begin_info{};
	SNK_CHECK_VK_RESULT(m_gbuffer_cmd_buffers[fif].buf->begin(begin_info));
	gbuffer_pass.RecordCommandBuffer(render_resources, scene, {p_window->GetWidth(), p_window->GetHeight()}, *m_gbuffer_cmd_buffers[fif].buf);
	SNK_CHECK_VK_RESULT(m_gbuffer_cmd_buffers[fif].buf->end());
	VkContext::GetLogicalDevice().SubmitGraphics(submit_info_graphics);

	SNK_CHECK_VK_RESULT(m_rt_cmd_buffers[fif].buf->begin(begin_info));

	render_resources.albedo_image.TransitionImageLayout(vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eColorAttachmentWrite,
		vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eRayTracingShaderKHR, 0, 1, 
		*m_rt_cmd_buffers[fif].buf);

	render_resources.normal_image.TransitionImageLayout(vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eColorAttachmentWrite,
		vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eRayTracingShaderKHR, 0, 1,
		*m_rt_cmd_buffers[fif].buf);

	render_resources.rma_image.TransitionImageLayout(vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eColorAttachmentWrite,
		vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eRayTracingShaderKHR, 0, 1,
		*m_rt_cmd_buffers[fif].buf);

	render_resources.depth_image.TransitionImageLayout(vk::ImageLayout::eDepthAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eDepthStencilAttachmentWrite,
		vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::PipelineStageFlagBits::eRayTracingShaderKHR, 0, 1,
		*m_rt_cmd_buffers[fif].buf);

	raytracing.RecordRenderCmdBuf(*m_rt_cmd_buffers[fif].buf, internal_render_image, scene, render_resources, m_render_settings.rt_settings);

	render_resources.albedo_image.TransitionImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eColorAttachmentOptimal, vk::AccessFlagBits::eShaderRead,
		vk::AccessFlagBits::eColorAttachmentWrite, vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::PipelineStageFlagBits::eColorAttachmentOutput, 0, 1,
		*m_rt_cmd_buffers[fif].buf);

	render_resources.normal_image.TransitionImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eColorAttachmentOptimal, vk::AccessFlagBits::eShaderRead,
		vk::AccessFlagBits::eColorAttachmentWrite, vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::PipelineStageFlagBits::eColorAttachmentOutput, 0, 1,
		*m_rt_cmd_buffers[fif].buf);

	render_resources.rma_image.TransitionImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eColorAttachmentOptimal, vk::AccessFlagBits::eShaderRead,
		vk::AccessFlagBits::eColorAttachmentWrite, vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::PipelineStageFlagBits::eColorAttachmentOutput, 0, 1,
		*m_rt_cmd_buffers[fif].buf);

	render_resources.depth_image.TransitionImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eDepthAttachmentOptimal, vk::AccessFlagBits::eDepthStencilAttachmentWrite,
		vk::AccessFlagBits::eShaderRead, vk::PipelineStageFlagBits::eEarlyFragmentTests, vk::PipelineStageFlagBits::eRayTracingShaderKHR, 0, 1,
		*m_rt_cmd_buffers[fif].buf);

	if (m_render_settings.dlss_preset != 0)
		m_streamline.EvaluateDLSS(*m_rt_cmd_buffers[fif].buf, render_resources, internal_render_image, display_render_image);

	//VkRenderer::RecordRenderDebugCommands(*m_cmd_buffers[VkContext::GetCurrentFIF()].buf, render_image, depth_image, 
	//	scene.GetSystem<SceneInfoBufferSystem>()->descriptor_buffers[VkContext::GetCurrentFIF()]);

	if (m_render_settings.using_taa) {
		m_taa_resolve_pass.RecordCommandBuffer(*m_rt_cmd_buffers[fif].buf);
		submit_info_graphics.pSignalSemaphores = &*m_compute_graphics_semaphore;
		submit_info_graphics.signalSemaphoreCount = 1;
	}

	SNK_CHECK_VK_RESULT(m_rt_cmd_buffers[fif].buf->end());

	graphics_cmd_buffers = util::array(*m_rt_cmd_buffers[fif].buf);
	submit_info_graphics.pCommandBuffers = graphics_cmd_buffers.data();
	VkContext::GetLogicalDevice().SubmitGraphics(submit_info_graphics);
	auto blit_cmd = *m_final_blit_cmd_buffers[fif].buf;
	blit_cmd.reset();
	SNK_CHECK_VK_RESULT(blit_cmd.begin(begin_info));
	if (m_render_settings.dlss_preset == 0) {
		internal_render_image.BlitTo(*swapchain_image, 0, 0, vk::ImageLayout::eGeneral, vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eColorAttachmentOptimal, vk::Filter::eNearest, std::nullopt, blit_cmd);
	}
	else {
		display_render_image.BlitTo(*swapchain_image, 0, 0, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eColorAttachmentOptimal, vk::Filter::eNearest, std::nullopt, blit_cmd);
	}
	SNK_CHECK_VK_RESULT(blit_cmd.end());
	
	submit_info_graphics.pCommandBuffers = &blit_cmd;
	submit_info_graphics.waitSemaphoreCount = 1;
	submit_info_graphics.pWaitSemaphores = &image_avail_semaphore;
	vk::PipelineStageFlags wait_stages = vk::PipelineStageFlagBits::eTransfer;
	submit_info_graphics.pWaitDstStageMask = &wait_stages;
	VkContext::GetLogicalDevice().SubmitGraphics(submit_info_graphics);

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
	RendererSettingsWindow();

	unsaved_changes |= asset_editor.RenderImGui();

	std::vector<EntityNode> entity_hierarchy = CreateLinearEntityHierarchy(&scene);
	if (ImGui::Begin("Entities")) {
		if (ImGui::TreeNode("Directional light")) {
			ent_editor.DirectionalLightEditor(scene.directional_light);
			ImGui::TreePop();
		}

		ImGui::InputInt("Temporal resamples", &m_render_settings.rt_settings.num_temporal_resamples);
		ImGui::InputInt("Spatial resamples", &m_render_settings.rt_settings.num_spatial_resamples);
		ImGui::Checkbox("Particles updating", &scene.GetSystem<ParticleSystem>()->active);
		PhysicsParamsUI();

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


void EditorLayer::RendererSettingsWindow() {
	static bool visible = false;
	if (p_window->input.IsKeyDown(Key::LeftControl) && p_window->input.IsKeyPressed('p')) {
		visible = !visible;
	}

	if (!visible)
		return;

	if (ImGui::Begin("Renderer settings")) {
		ImGui::Checkbox("TAA", &m_render_settings.using_taa);

		const char* dlss_presets[] = {"Off", "Max performance", "Balanced", "Max quality", "Ultra performance", "Ultra quality", "DLAA"};
		if (StreamlineWrapper::IsAvailable() && ImGui::Combo("DLSS Preset", &m_render_settings.dlss_preset, dlss_presets, IM_ARRAYSIZE(dlss_presets))) {
			m_render_settings.realloc_render_resources = true;
		}

		if (ImGui::InputInt("MQP", &m_render_settings.dlss_quality_preset))
			m_render_settings.realloc_render_resources = true;
	}
	ImGui::End();
}

void EditorLayer::OnShutdown() {}