#pragma once
#include "util/util.h"

#include "core/Window.h"
namespace SNAKE {
	struct Layer {
		virtual ~Layer() = default;

		virtual void OnInit() = 0;

		// Render/update threads synchronized at this stage, synced reads/writes should happen here
		virtual void OnFrameStart() = 0;

		// Runs concurrently with OnRender
		virtual void OnUpdate() = 0;

		// Runs concurrently with OnUpdate
		virtual void OnRender() = 0;

		virtual void OnImGuiRender() = 0;
		virtual void OnShutdown() = 0;
	};


	class LayerManager {
	public:
		LayerManager(class Window* _window) : mp_window(_window) {};

		void PushLayer(Layer* p_layer) {
			SNK_ASSERT(!util::VectorContains(p_layer, m_layers));
			m_layers.push_back(p_layer);
			
		}

		void RemoveLayer(Layer* p_layer) {
			auto it = std::ranges::find(m_layers, p_layer);
			if (it == m_layers.end()) {
				SNK_CORE_ERROR("LayerManager::RemoveLayer failed, layer not found in LayerManager");
				return;
			}

			m_layers.erase(it);
		}

		void InitLayers();

		void OnFrameStart() {
			for (auto* p_layer : m_layers) {
				p_layer->OnFrameStart();
			}
		}

		void OnUpdate() {
			for (auto* p_layer : m_layers) {
				p_layer->OnUpdate();
			}
		}

		void OnImGuiRender();

		void OnRender() {
			for (auto* p_layer : m_layers) {
				p_layer->OnRender();
			}
		}

		void ShutdownLayers() {
			for (auto* p_layer : m_layers) {
				p_layer->OnShutdown();
				delete p_layer;
			}

			m_layers.clear();
		}

	private:
		class Window* mp_window = nullptr;
		class ImGuiLayer* mp_imgui_layer = nullptr;

		std::vector<Layer*> m_layers;
	};
}