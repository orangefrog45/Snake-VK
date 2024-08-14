#pragma once
#include "layers/LayerManager.h"
#include "scene/Scene.h"
#include "rendering/VkSceneRenderer.h"

namespace SNAKE {
	class Window;

	class EditorLayer : public Layer {
	public:
		EditorLayer(Window* _window) : p_window(_window) {};
		void OnInit() override;
		void OnUpdate() override;
		void OnRender() override;
		void OnImGuiRender() override;
		void OnShutdown() override;
	private:
		Scene scene;
		Entity* p_cam_ent = nullptr;

		VkSceneRenderer renderer;

		Window* p_window = nullptr;

	};
}