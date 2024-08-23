#include "pch/pch.h"
#include "assets/AssetManager.h"
#include "AssetEditor.h"
#include "core/Window.h"
#include "EditorLayer.h"
#include "util/util.h"
#include "util/UI.h"
#include "util/FileUtil.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
using namespace SNAKE;

void AssetEditor::Init() {
	auto tex = AssetManager::GetAsset<Texture2DAsset>(AssetManager::CoreAssetIDs::TEXTURE);

	renderer.Init(*p_window, &p_editor->scene);

	Image2DSpec spec;
	spec.aspect_flags = vk::ImageAspectFlagBits::eColor;
	spec.format = vk::Format::eR8G8B8A8Srgb;
	spec.size = { 250, 250 };
	spec.tiling = vk::ImageTiling::eOptimal;
	spec.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;

	render_image.SetSpec(spec);
	render_image.CreateImage();
	render_image.CreateImageView();
	render_image.CreateSampler();

	render_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, 
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

	render_image_set = ImGui_ImplVulkan_AddTexture(render_image.GetSampler(), render_image.GetImageView(), (VkImageLayout)vk::ImageLayout::eShaderReadOnlyOptimal);
}


void AssetEditor::RenderAssetEntry(const AssetEntry& entry) {
	ImGui::PushID(entry.p_asset);

	ImGui::Text(entry.asset_type_name.c_str());
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


bool AssetEditor::RenderBaseAssetEditor() {
	if (!p_selected_asset)
		return false;

	bool ret = false;
	ImGui::PushID(p_selected_asset);

	ImGui::SetNextWindowSize({ 500, 500 });
	if (ImGui::Begin("Asset editor")) {
		ImGui::Dummy({ 0, 0 }); ImGui::SameLine(ImGui::GetContentRegionAvail().x - 5); if (ImGui::SmallButton("X")) p_selected_asset = nullptr;
		ImGui::Text("Name: "); ImGui::InputText("##name", &p_selected_asset->name);
		ImGui::Text(std::format("Filepath: {}", p_selected_asset->filepath).c_str());
		ImGui::ImageButton(GetOrCreateAssetImage(p_selected_asset), {150, 150});

		if (dynamic_cast<MaterialAsset*>(p_selected_asset)) {
			ret |= RenderMaterialEditor();
		}

	}
	ImGui::End();

	ImGui::PopID();

	return ret;
}

void AssetEditor::Render() {
	return;

	render_image.TransitionImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eColorAttachmentOptimal,
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eColorAttachmentOutput, 
		vk::PipelineStageFlagBits::eBottomOfPipe);

	renderer.RenderScene(render_image);
	render_image.TransitionImageLayout(vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone,
		vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eFragmentShader);
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
	auto base_image = GetOrCreateAssetImage(AssetManager::GetAssetRaw<Texture2DAsset>(AssetManager::CoreAssetIDs::TEXTURE));
	ImGui::TableNextColumn();
	ImGui::Image(render_image_set, { 100, 100 });
	for (auto* mesh : meshes) {
		ImGui::TableNextColumn();
		AssetEntry entry;
		entry.image = base_image;
		entry.drag_drop_name = "MESH";
		entry.asset_type_name = "Static mesh";
		entry.p_asset = mesh;
		RenderAssetEntry(entry);
	}

	auto mesh_datas = AssetManager::GetView<MeshDataAsset>();
	for (auto* data : mesh_datas) {
		ImGui::TableNextColumn();
		AssetEntry entry;
		entry.image = base_image;
		entry.drag_drop_name = "MESH_DATA";
		entry.asset_type_name = "Mesh data";
		entry.p_asset = data;

		entry.popup_settings["Create static mesh from"] = [&] {
			auto* p_box = p_editor->CreateDialogBox();
			p_box->block_other_window_input = true;

			p_box->imgui_render_cb = [data, p_box] {
				static std::string new_mesh_name = "New mesh";
				ImGui::Text("Name: "); ImGui::SameLine(); ImGui::InputText("##NAME", &new_mesh_name);
				if (ImGui::Button("Create")) {
					p_box->close = true;
					auto new_mesh = AssetManager::CreateAsset<StaticMeshAsset>();
					new_mesh->data = *data;
					new_mesh->name = new_mesh_name;
				}
			};

		};

		RenderAssetEntry(entry);
	}

}



void AssetEditor::RenderMaterials() {
	auto materials = AssetManager::GetView<MaterialAsset>();
	for (auto* material : materials) {
		ImGui::TableNextColumn();
		AssetEntry entry;
		entry.image = GetOrCreateAssetImage(material);
		entry.drag_drop_name = "MATERIAL";
		entry.p_asset = material;
		RenderAssetEntry(entry);
	}
}

void AssetEditor::RenderTextures() {
	auto textures = AssetManager::GetView<Texture2DAsset>();
	auto base_image = GetOrCreateAssetImage(AssetManager::GetAssetRaw<Texture2DAsset>(AssetManager::CoreAssetIDs::TEXTURE));

	for (auto* tex : textures) {
		ImGui::TableNextColumn();
		AssetEntry entry;
		entry.image = GetOrCreateAssetImage(tex);
		entry.drag_drop_name = "TEXTURE2D";
		entry.p_asset = tex;
		RenderAssetEntry(entry);
	}
}

enum class AssetType {
	MESH,
	TEXTURE,
	MATERIAL
};

vk::DescriptorSet AssetEditor::GetOrCreateAssetImage(Asset* _asset) {
	if (auto* p_asset = dynamic_cast<MaterialAsset*>(_asset)) {
		if (!p_asset->albedo_tex)
			return GetOrCreateAssetImage(AssetManager::GetAssetRaw<Texture2DAsset>(AssetManager::CoreAssetIDs::TEXTURE));

		if (!asset_images.contains(p_asset->albedo_tex.get()))
			asset_images[p_asset->albedo_tex.get()] = ImGui_ImplVulkan_AddTexture(p_asset->albedo_tex->image.GetSampler(), p_asset->albedo_tex->image.GetImageView(), (VkImageLayout)vk::ImageLayout::eShaderReadOnlyOptimal);

		return asset_images[p_asset->albedo_tex.get()];
	}
	else if (auto* p_asset = dynamic_cast<Texture2DAsset*>(_asset)) {
		if (!asset_images.contains(p_asset))
			asset_images[p_asset] = ImGui_ImplVulkan_AddTexture(p_asset->image.GetSampler(), p_asset->image.GetImageView(), (VkImageLayout)vk::ImageLayout::eShaderReadOnlyOptimal);

		return asset_images[p_asset];
	}
	else {
		return GetOrCreateAssetImage(AssetManager::GetAssetRaw<Texture2DAsset>(AssetManager::CoreAssetIDs::TEXTURE));
	}
}


Asset* AssetEditor::AddAssetButton() {
	static std::unordered_map <std::string, std::function<void()>> popup_options;
	Asset* ret = nullptr;

	popup_options["Mesh data"] = [this, &ret] {
		std::string filepath = files::SelectFileFromExplorer();
		if (!filepath.empty()) {
			auto mesh_data = AssetManager::CreateAsset<MeshDataAsset>();
			mesh_data->filepath = filepath;
			if (!AssetManager::LoadMeshFromFile(mesh_data)) {
				AssetManager::DeleteAsset(mesh_data.get());
				p_editor->ErrorMessagePopup(std::format("Failed to load mesh file '{}', check logs for more info", filepath));
			}
			else {
				ret = mesh_data.get();
			}
		}
	};

	popup_options["Texture"] = [this, &ret] {
		std::string filepath = files::SelectFileFromExplorer();
		if (!filepath.empty()) {
			auto tex = AssetManager::CreateAsset<Texture2DAsset>();
			tex->filepath = filepath;
			if (!AssetManager::LoadTextureFromFile(tex)) {
				AssetManager::DeleteAsset(tex.get());
				p_editor->ErrorMessagePopup(std::format("Failed to load texture file '{}', check logs for more info", filepath));
			}
			else {
				ret = tex.get();
			}
		}
	};

	popup_options["Material"] = [this, &ret] {
		ret = AssetManager::CreateAsset<MaterialAsset>().get();
	};

	ImGui::Button("Add asset");
	ImGuiUtil::Popup("##add asset", popup_options, ImGui::IsItemClicked());

	return ret;
}

bool AssetEditor::RenderImGui() {
	bool ret = false;

	ret |= RenderBaseAssetEditor();

	constexpr unsigned ASSET_WINDOW_HEIGHT = 200;

	ImGui::SetNextWindowPos({ 0, (float)p_window->GetHeight() - ASSET_WINDOW_HEIGHT });
	ImGui::SetNextWindowSize({ (float)p_window->GetWidth(), ASSET_WINDOW_HEIGHT });
	if (ImGui::Begin("Assets", nullptr)) {
		auto meshes = AssetManager::GetView<MeshDataAsset>();

		unsigned num_cols = glm::max((int)p_window->GetWidth() / 125, 1);
		if (ImGui::BeginTable("Asset window separator", 2, ImGuiTableFlags_SizingFixedFit)) {
			ImGui::TableNextColumn();

			static AssetType current_asset_type_selected = AssetType::MESH;
			auto asset_types = util::array("Meshes", "Textures", "Materials");
			for (size_t i = 0; i < asset_types.size(); i++) {
				bool selected = static_cast<size_t>(current_asset_type_selected) == i;
				if (ImGui::Selectable(asset_types[i], &selected, 0, {400, ASSET_WINDOW_HEIGHT / asset_types.size()})) {
					current_asset_type_selected = static_cast<AssetType>(i);
				}
			}

			ImGui::TableNextColumn();
			AddAssetButton();
			if (ImGui::BeginTable("asset_table", num_cols, ImGuiTableFlags_Borders, {125.f * num_cols, ASSET_WINDOW_HEIGHT }, 125.f * num_cols)) {
				switch (current_asset_type_selected) {
				case AssetType::MESH:
					RenderMeshes();
					break;
				case AssetType::TEXTURE:
					RenderTextures();
					break;
				case AssetType::MATERIAL:
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
