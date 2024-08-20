#include "pch/pch.h"
#include "AssetEditor.h"
#include "util/util.h"
#include "core/Window.h"
#include "assets/AssetManager.h"
#include "util/UI.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <backends/imgui_impl_vulkan.h>
using namespace SNAKE;

void AssetEditor::Init() {
	auto tex = AssetManager::GetAsset<Texture2DAsset>(AssetManager::CoreAssetIDs::TEXTURE);
	set = ImGui_ImplVulkan_AddTexture(tex->image.GetSampler(), tex->image.GetImageView(), (VkImageLayout)vk::ImageLayout::eShaderReadOnlyOptimal);
}


void AssetEditor::RenderAssetEntry(const AssetEntry& entry) {
	ImGui::PushID(entry.p_asset);

	ImGui::Text(entry.p_asset->name.c_str());
	ImGui::Text(std::format("Refs: {}", entry.p_asset->GetRefCount()).c_str());
	if (ImGui::ImageButton(entry.image, { 100, 100 })) p_selected_asset = entry.p_asset;


	static uint64_t drag_drop_asset_uuid = INVALID_UUID;

	if (ImGui::BeginDragDropSource()) {
		drag_drop_asset_uuid = entry.p_asset->uuid();
		SNK_CORE_TRACE(drag_drop_asset_uuid);
		ImGui::SetDragDropPayload(entry.drag_drop_name.c_str(), &drag_drop_asset_uuid, sizeof(uint64_t));
		ImGui::EndDragDropSource();
	}
	
	ImGuiUtil::Popup("Asset options", entry.popup_settings, ImGui::IsItemClicked(1));

	ImGui::PopID();
}


bool AssetEditor::RenderBaseAssetEditor(const AssetEntry& entry) {
	if (!p_selected_asset)
		return false;

	bool ret = false;
	ImGui::PushID(&entry);

	ImGui::SetNextWindowSize({ 500, 500 });
	if (ImGui::Begin("Asset editor")) {
		ImGui::Dummy({ 0, 0 }); ImGui::SameLine(ImGui::GetContentRegionAvail().x - 5); if (ImGui::SmallButton("X")) p_selected_asset = nullptr;
		ImGui::Text("Name: "); ImGui::InputText("##name", &entry.p_asset->name);
		ImGui::Text(std::format("Filepath: {}", entry.p_asset->filepath).c_str());
		ImGui::ImageButton(entry.image, { 150, 150 });

		if (dynamic_cast<MaterialAsset*>(p_selected_asset)) {
			ret |= RenderMaterialEditor();
		}
	}
	ImGui::End();

	ImGui::PopID();

	return ret;
}

bool AssetEditor::RenderMaterialEditor() {
	bool ret = false;
	auto* p_mat = dynamic_cast<MaterialAsset*>(p_selected_asset);

	ret |= ImGui::InputFloat3("Albedo", &p_mat->albedo.r);
	ret |= ImGui::InputFloat("Roughness", &p_mat->roughness);
	ret |= ImGui::InputFloat("Metallic", &p_mat->metallic);
	ret |= ImGui::InputFloat("AO", &p_mat->ao);

	if (ImGui::BeginTable("##material-table", 5)) {
		ImGui::TableNextColumn();
		ImGui::Text("Albedo");
		if (auto new_tex = RenderMaterialTexture(p_mat->albedo_tex.get()); new_tex.has_value())
		{
			ret = true;
			p_mat->albedo_tex = new_tex.value();
		};
		ImGui::TableNextColumn();
		ImGui::Text("Normal");
		if (auto new_tex = RenderMaterialTexture(p_mat->normal_tex.get()); new_tex.has_value())
		{
			ret = true;
			p_mat->normal_tex = new_tex.value();
		};
		ImGui::TableNextColumn();
		ImGui::Text("Roughness");
		if (auto new_tex = RenderMaterialTexture(p_mat->roughness_tex.get()); new_tex.has_value())
		{
			ret = true;
			p_mat->roughness_tex = new_tex.value();
		};
		ImGui::TableNextColumn();
		ImGui::Text("Metallic");
		if (auto new_tex = RenderMaterialTexture(p_mat->metallic_tex.get()); new_tex.has_value())
		{
			ret = true;
			p_mat->metallic_tex = new_tex.value();
		};
		ImGui::TableNextColumn();
		ImGui::Text("AO");  
		if ( auto new_tex = RenderMaterialTexture(p_mat->ao_tex.get()); new_tex.has_value()) 
		{ 
			ret = true; 
			p_mat->ao_tex = new_tex.value();
		};
		ImGui::EndTable();
	}

	if (ret) p_mat->DispatchUpdateEvent();
	return ret;
}

std::optional<Texture2DAsset*> AssetEditor::RenderMaterialTexture(Texture2DAsset* p_tex) {
	if (!p_tex)
		ImGui::ImageButton(GetOrCreateAssetImage(AssetManager::GetAsset<Texture2DAsset>(AssetManager::CoreAssetIDs::TEXTURE).get()), {100, 100});
	else
		ImGui::ImageButton(GetOrCreateAssetImage(p_tex), { 100, 100 });

	if (ImGui::IsItemClicked(1)) {
		return nullptr;
	}

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE2D")) {
			auto id = *reinterpret_cast<uint64_t*>(payload->Data);
			return AssetManager::GetAsset<Texture2DAsset>(id).get();
		}
		ImGui::EndDragDropTarget();
	}

	return std::nullopt;
}


void AssetEditor::RenderMeshes() {
	auto meshes = AssetManager::GetView<StaticMeshAsset>();
	for (auto& mesh : meshes) {
		ImGui::TableNextColumn();
		AssetEntry entry;
		entry.image = set;
		entry.drag_drop_name = "MESH";
		entry.p_asset = mesh;
		RenderAssetEntry(entry);
	}
}



void AssetEditor::RenderMaterials() {
	auto materials = AssetManager::GetView<MaterialAsset>();
	for (auto& material : materials) {
		ImGui::TableNextColumn();
		AssetEntry entry;
		entry.image = set;
		entry.drag_drop_name = "MATERIAL";
		entry.p_asset = material;
		RenderAssetEntry(entry);
	}
}

void AssetEditor::RenderTextures() {
	auto textures = AssetManager::GetView<Texture2DAsset>();
	for (auto& tex : textures) {
		ImGui::TableNextColumn();
		AssetEntry entry;
		entry.image = set;
		entry.drag_drop_name = "TEXTURE2D";
		entry.p_asset = tex;
		RenderAssetEntry(entry);
	}
}

bool AssetEditor::RenderImGui() {
	bool ret = false;

	AssetEntry entry;
	entry.image = set;
	entry.p_asset = p_selected_asset;
	ret |= RenderBaseAssetEditor(entry);

	constexpr unsigned ASSET_WINDOW_HEIGHT = 200;

	ImGui::SetNextWindowPos({ 0, (float)p_window->GetHeight() - ASSET_WINDOW_HEIGHT });
	ImGui::SetNextWindowSize({ (float)p_window->GetWidth(), ASSET_WINDOW_HEIGHT });
	if (ImGui::Begin("Assets", nullptr)) {
		auto meshes = AssetManager::GetView<StaticMeshDataAsset>();

		unsigned num_cols = glm::max((int)p_window->GetWidth() / 125, 1);
		if (ImGui::BeginTable("Asset window separator", 2, ImGuiTableFlags_SizingFixedFit)) {
			ImGui::TableNextColumn();

			static size_t current_asset_type_selected = 0;
			auto asset_types = util::array("Meshes", "Textures", "Materials");
			for (size_t i = 0; i < asset_types.size(); i++) {
				bool selected = current_asset_type_selected == i;
				if (ImGui::Selectable(asset_types[i], &selected, 0, {400, ASSET_WINDOW_HEIGHT / asset_types.size()})) {
					current_asset_type_selected = i;
				}
			}

			ImGui::TableNextColumn();
			if (ImGui::BeginTable("asset_table", num_cols, ImGuiTableFlags_Borders, {125.f * num_cols, ASSET_WINDOW_HEIGHT }, 125.f * num_cols)) {
				switch (current_asset_type_selected) {
				case 0:
					RenderMeshes();
					break;
				case 1:
					RenderTextures();
					break;
				case 2:
					RenderMaterials();
					break;
				}

				ImGui::EndTable();
			}

			ImGui::EndTable();
		}

		ImGui::End();
	}

	return ret;
}
