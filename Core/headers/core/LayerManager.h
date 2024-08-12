#pragma once
#include "util/util.h"

namespace SNAKE {
	struct Layer {
		virtual void OnInit() = 0;
		virtual void OnUpdate() = 0;
		virtual void OnRender() = 0;
		virtual void OnImGuiRender() = 0;
		virtual void OnShutdown() = 0;
	};

	class LayerManager {
	public:
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

		void InitLayers() {
			for (auto* p_layer : m_layers) {
				p_layer->OnInit();
			}
		}

		void OnUpdate() {
			for (auto* p_layer : m_layers) {
				p_layer->OnUpdate();
			}
		}

		void OnRender() {
			for (auto* p_layer : m_layers) {
				p_layer->OnRender();
			}
		}

		void ShutdownLayers() {
			for (auto* p_layer : m_layers) {
				p_layer->OnShutdown();
			}
		}

	private:
		std::vector<Layer*> m_layers;
	};
}