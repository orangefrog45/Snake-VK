#pragma once

#include "LayerManager.h"

namespace SNAKE {
	class ImGuiLayer : public Layer {
	public:
		ImGuiLayer(Window* p_window) : mp_window(p_window) {};

		void OnInit();
		void OnShutdown();

		void OnImGuiStartRender();
		void OnImGuiEndRender();

		void OnUpdate() {};
		void OnRender() {};
		void OnImGuiRender() {};

	private:
		vk::UniqueDescriptorPool m_imgui_descriptor_pool;

		Window* mp_window = nullptr;
	};
}