#pragma once
#include "AssetEditor.h"
#include "EntityEditor.h"
#include "layers/LayerManager.h"
#include "rendering/VkSceneRenderer.h"
#include "rendering/Raytracing.h"
#include "rendering/RenderCommon.h"
#include "renderpasses/GBufferPass.h"
#include "renderpasses/StreamlinePasses.h"
#include "renderpasses/TAA_ResolvePass.h"
#include "scene/Scene.h"

namespace SNAKE {
	class Window;

	struct DialogBox {
		std::string name = "Dialog box";
		glm::ivec2 dimensions = { 600, 300 };
		std::function<void()> imgui_render_cb;
		std::any data;
		bool close = false;
		bool block_other_window_input = true;
	};

	struct ProjectState {
		std::string directory;
		std::string active_scene_path;
	};

	class EditorLayer : public Layer {
	public:
		EditorLayer(Window* _window) : p_window(_window), ent_editor(&scene), asset_editor(p_window, this) {};
		void OnInit() override;
		void OnFrameStart() override;
		void OnUpdate() override;
		void OnRender() override;
		void OnImGuiRender() override;
		void OnShutdown() override;

		[[nodiscard]] DialogBox* CreateDialogBox();
		void ErrorMessagePopup(const std::string& err);

		Scene scene;

		Image2D internal_render_image;
		// Not created/used unless DLSS is active
		Image2D display_render_image;

		RaytracingResources render_resources;

		ProjectState project;

		RT raytracing;
	private:
		void InitRenderResources();
		void InitGBuffer(glm::vec2 internal_render_dim);

		bool CreateEntityPopup(bool open_condition);
		bool ModifyEntityPopup(bool open_condition, Entity* p_ent);

		void LoadProject(const std::string& project_path);
		void SaveProject();

		void PromptCreateNewProject();
		void CreateProject(const std::string& directory, const std::string& project_name);

		void ToolbarGUI();
		void PhysicsParamsUI();
		void RendererSettingsWindow();
		void RenderDialogBoxes();

		// TODO: move these elsewhere
		glm::mat4 m_prev_frame_projection_matrix;
		glm::mat4 m_prev_frame_view_matrix;
		glm::mat4 m_prev_frame_view_to_world_matrix;

		vk::UniqueSemaphore m_compute_graphics_semaphore;

		TAA_ResolvePass m_taa_resolve_pass{ &internal_render_image, &render_resources.pixel_motion_image };
		StreamlinePasses m_streamline;

		EventListener m_window_resize_listener;

		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_gbuffer_cmd_buffers;
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_rt_cmd_buffers;
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_final_blit_cmd_buffers;

		struct RenderSettings {
			bool realloc_render_resources = false;
			// Just window dimensions if upscaling/DLSS is disabled
			glm::vec2 internal_render_dim = { 0, 0 };

			bool using_taa = false;
			int dlss_preset = 0;
			int dlss_quality_preset = 0;
		} m_render_settings;

		Window* p_window = nullptr;

		AssetEditor asset_editor;

		std::string editor_executable_dir;
		bool unsaved_changes = false;

		std::unique_ptr<Entity> p_cam_ent = nullptr;

		std::vector<std::unique_ptr<DialogBox>> dialog_boxes;
		std::vector<std::unique_ptr<DialogBox>> new_dialog_boxes_to_sort;

		// Entities which nodes have been opened in the entity viewer panel, revealing their children
		std::unordered_set<Entity*> open_entity_nodes;

		EntityEditor ent_editor;

		GBufferPass gbuffer_pass;

		EventListener entity_deletion_listener;
	};
}