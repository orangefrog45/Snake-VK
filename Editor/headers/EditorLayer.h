#pragma once
#include "EntityEditor.h"
#include "layers/LayerManager.h"
#include "rendering/VkSceneRenderer.h"
#include "scene/Scene.h"
#include "AssetEditor.h"
#include "rendering/Raytracing.h"

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
		void OnUpdate() override;
		void OnRender() override;
		void OnImGuiRender() override;
		void OnShutdown() override;

		[[nodiscard]] DialogBox* CreateDialogBox();
		void ErrorMessagePopup(const std::string& err);

		Scene scene;
		FullscreenImage2D render_image;
		FullscreenImage2D depth_image;

		ProjectState project;

		RT raytracing;
	private:
		void RenderDialogBoxes();

		bool CreateEntityPopup(bool open_condition);
		bool ModifyEntityPopup(bool open_condition, Entity* p_ent);

		void LoadProject(const std::string& project_path);
		void SaveProject();

		void ToolbarGUI();
		
		void PromptCreateNewProject();
		void CreateProject(const std::string& directory, const std::string& project_name);

		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;

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

		VkSceneRenderer renderer;

		EventListener entity_deletion_listener;
	};
}