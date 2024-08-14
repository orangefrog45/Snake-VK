#include "layers/LayerManager.h"
#include "layers/ImGuiLayer.h"
#include "rendering/VkRenderer.h"

using namespace SNAKE;

void LayerManager::InitLayers() {
	mp_imgui_layer = new ImGuiLayer(mp_window);
	mp_imgui_layer->OnInit();

	for (auto* p_layer : m_layers) {
		p_layer->OnInit();
	}
	PushLayer(mp_imgui_layer);
}


void LayerManager::OnImGuiRender() {
	if (VkRenderer::GetCurrentSwapchainImageIndex() == VkRenderer::READY_TO_REACQUIRE_SWAPCHAIN_IDX) // No image to render to acquired
		return;

	mp_imgui_layer->OnImGuiStartRender();
	for (auto* p_layer : m_layers) {
		p_layer->OnImGuiRender();
	}
	mp_imgui_layer->OnImGuiEndRender();
	VkRenderer::RenderImGuiAndPresent(*mp_window, *mp_window->GetVkContext().swapchain_images[VkRenderer::GetCurrentSwapchainImageIndex()], { mp_window->GetWidth(), mp_window->GetHeight() });
}

