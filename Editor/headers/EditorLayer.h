#pragma once
#include "EntityEditor.h"
#include "layers/LayerManager.h"
#include "rendering/VkSceneRenderer.h"
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

	class EditorLayer : public Layer {
	public:
		EditorLayer(Window* _window) : p_window(_window), ent_editor(&scene) {};
		void OnInit() override;
		void OnUpdate() override;
		void OnRender() override;
		void OnImGuiRender() override;
		void OnShutdown() override;

		[[nodiscard]] DialogBox* CreateDialogBox();
	private:
		void RenderDialogBoxes();

		void LoadProject(const std::string& project_path);
		void LoadProjectSettings(const std::string& settings_file);

		void ToolbarGUI();
		
		void PromptCreateNewProject();
		void CreateProject(const std::string& directory, const std::string& project_name);

		std::string active_project_path;

		Scene scene;
		Entity* p_cam_ent = nullptr;

		std::vector<DialogBox> dialog_boxes;

		// Entities which nodes have been opened in the entity viewer panel, revealing their children
		std::unordered_set<Entity*> open_entity_nodes;

		EntityEditor ent_editor;

		VkSceneRenderer renderer;

		Window* p_window = nullptr;

	};
}