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
		bool RenderImGui();

		vk::DescriptorSet set;
		void RenderAssetEntry(const AssetEntry& entry);
	private:
		template<typename T>
		vk::DescriptorSet GetOrCreateAssetImage(T* p_asset) {
			if constexpr (std::is_same_v<T, MaterialAsset>) {
				if (!asset_images.contains(p_asset->albedo_tex.get())) {
					asset_images[p_asset->albedo_tex.get()] = ImGui_ImplVulkan_AddTexture(p_asset->albedo_tex->image.GetSampler(), p_asset->albedo_tex->image.GetImageView(), (VkImageLayout)vk::ImageLayout::eShaderReadOnlyOptimal);
				}

				return asset_images[p_asset->albedo_tex.get()];
			} else if constexpr (std::is_same_v<T, Texture2DAsset>) {
				if (!asset_images.contains(p_asset)) {
					asset_images[p_asset] = ImGui_ImplVulkan_AddTexture(p_asset->image.GetSampler(), p_asset->image.GetImageView(), (VkImageLayout)vk::ImageLayout::eShaderReadOnlyOptimal);
				}

				return asset_images[p_asset];
			}
			else if constexpr (std::is_same_v<T, StaticMeshAsset>) {

			}
		}
		

		bool RenderMaterialEditor();

		// Returns new texture if a new texture was drag/dropped onto this display
		std::optional<Texture2DAsset*> RenderMaterialTexture(Texture2DAsset* p_tex);

		void RenderMeshes();
		void RenderTextures();
		void RenderMaterials();

		bool RenderBaseAssetEditor(const AssetEntry& entry);

		std::unordered_map<Asset*, vk::DescriptorSet> asset_images;

		Asset* p_selected_asset = nullptr;

		class Window* p_window = nullptr;
	};
}