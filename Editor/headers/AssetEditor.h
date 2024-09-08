#pragma once
#include "core/VkIncl.h"
#include "assets/MaterialAsset.h"
#include "assets/AssetManager.h"
#include <backends/imgui_impl_vulkan.h>
#include "rendering/VkSceneRenderer.h"

namespace SNAKE {
	struct AssetEntry {
		std::string asset_type_name;

		std::string drag_drop_name;
		
		std::unordered_map<std::string, std::function<void()>> popup_settings;

		vk::DescriptorSet image;

		Asset* p_asset = nullptr;
	};

	class AssetEditor {
	public:
		AssetEditor(class Window* _window, class EditorLayer* _editor) : p_window(_window), p_editor(_editor) {};
		void Init();
		void Render();
		bool RenderImGui();

		// Serializes all assets, overwriting their existing serialized files in the active project resource folder
		void SerializeAllAssets();
		void DeserializeAllAssetsFromActiveProject();

		void RenderAssetEntry(AssetEntry& entry);
		vk::DescriptorSet GetOrCreateAssetImage(Asset* _asset);
	private:
		void DeleteAsset(Asset* p_asset);

		void OnRequestTextureAssetAddFromFile(const std::string& filepath);
		bool OnRequestMeshAssetAddFromFile(const std::string& filepath, const std::string& name);

		bool RenderMaterialEditor();

		VkSceneRenderer renderer;
		Image2D render_image;
		vk::DescriptorSet render_image_set;

		std::vector<Asset*> asset_deletion_queue;

		// Returns new texture if a new texture was drag/dropped onto this display
		std::optional<Texture2DAsset*> RenderMaterialTexture(Texture2DAsset* p_tex);


		void RenderMeshes();
		void RenderTextures();
		void RenderMaterials();

		bool AddAssetButton();
		bool RenderBaseAssetEditor();

		std::unordered_map<Asset*, vk::DescriptorSet> asset_images;

		EventListener asset_deletion_listener;

		Asset* p_selected_asset = nullptr;

		EditorLayer* p_editor = nullptr;
		Window* p_window = nullptr;
	};
}