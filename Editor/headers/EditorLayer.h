#pragma once
#include "EntityEditor.h"
#include "layers/LayerManager.h"
#include "rendering/VkSceneRenderer.h"
#include "scene/Scene.h"

namespace SNAKE {
	class Window;

	class EditorLayer : public Layer {
	public:
		EditorLayer(Window* _window) : p_window(_window), ent_editor(&scene) {};
		void OnInit() override;
		void OnUpdate() override;
		void OnRender() override;
		void OnImGuiRender() override;
		void OnShutdown() override;
	private:
		Scene scene;
		Entity* p_cam_ent = nullptr;

		// Entities which nodes have been opened in the entity viewer panel, revealing their children
		std::unordered_set<Entity*> open_entity_nodes;

		EntityEditor ent_editor;

		VkSceneRenderer renderer;

		Window* p_window = nullptr;

	};
}