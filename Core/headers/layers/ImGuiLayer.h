#pragma once

#include "LayerManager.h"

namespace SNAKE {
	class ImGuiLayer : public Layer {
	public:
		ImGuiLayer(Window* p_window) : mp_window(p_window) {};

		void OnInit() override;
		void OnFrameStart() override {};
		void OnShutdown() override;

		void OnImGuiStartRender();
		void OnImGuiEndRender();

		void OnUpdate() override {};
		void OnRender() override {};
		void OnImGuiRender() override {};

	private:
		vk::UniqueDescriptorPool m_imgui_descriptor_pool;

		Window* mp_window = nullptr;
	};
}