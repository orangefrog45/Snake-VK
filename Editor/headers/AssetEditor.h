#pragma once
#include "core/VkIncl.h"
#include "assets/MaterialAsset.h"
#include "assets/AssetManager.h"

namespace SNAKE {
	struct AssetEntry {
		std::string drag_drop_name;
		
		std::unordered_map<std::string, std::function<void()>> popup_settings = { {"Delete", [&] { AssetManager::DeleteAsset(p_asset); }} };

		vk::DescriptorSet image;

		Asset* p_asset = nullptr;
	};

	class AssetEditor {
	public:
		AssetEditor(class Window* _window) : p_window(_window) {};
		void Init();
		void RenderImGui();

		vk::DescriptorSet set;
		void RenderAssetEntry(const AssetEntry& entry);
	private:
		void RenderMaterialEditor();

		void RenderMeshes();
		void RenderTextures();
		void RenderMaterials();

		void RenderBaseAssetEditor(const AssetEntry& entry);


		Asset* p_selected_asset = nullptr;

		class Window* p_window = nullptr;
	};
}