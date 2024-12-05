#include "pch/pch.h"
#include "assets/AssetManager.h"
#include "AssetEditor.h"
#include "core/Window.h"
#include "EditorLayer.h"
#include "util/util.h"
#include "util/UI.h"
#include "util/FileUtil.h"
#include "assets/AssetLoader.h"
#include "util/ByteSerializer.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
using namespace SNAKE;

void AssetEditor::Init() {
	asset_deletion_listener.callback = [this](Event const* _event) {
		auto* p_casted = dynamic_cast<AssetEvent const*>(_event);

		if (asset_images.contains(p_casted->p_asset))
			asset_images.erase(p_casted->p_asset);

		if (p_selected_asset == p_casted->p_asset)
			p_selected_asset = nullptr;
	};

	EventManagerG::RegisterListener<AssetEvent>(asset_deletion_listener);

	auto tex = AssetManager::GetAsset<Texture2DAsset>(AssetManager::CoreAssetIDs::TEXTURE);

	renderer.Init(&p_editor->scene);

	Image2DSpec spec;
	spec.aspect_flags = vk::ImageAspectFlagBits::eColor;
	spec.format = vk::Format::eR8G8B8A8Srgb;
	spec.size = { 250, 250 };
	spec.tiling = vk::ImageTiling::eOptimal;
	spec.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;

	render_image.SetSpec(spec);
	render_image.CreateImage();

	render_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, 
		vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

	render_image_set = ImGui_ImplVulkan_AddTexture(render_image.GetSampler(), render_image.GetImageView(), (VkImageLayout)vk::ImageLayout::eShaderReadOnlyOptimal);
}


void AssetEditor::RenderAssetEntry(AssetEntry& entry) {
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
	
	entry.popup_settings["Delete"] = [&] {	DeleteAsset(entry.p_asset); };
	ImGuiWidgets::Popup("Asset options", entry.popup_settings, ImGui::IsItemClicked(1));

	ImGui::PopID();
}


bool AssetEditor::RenderBaseAssetEditor() {
	if (!p_selected_asset)
		return false;

	bool ret = false;
	ImGui::PushID(p_selected_asset);

	ImGui::SetNextWindowSize({ 500, 500 });
	if (ImGui::Begin("Asset editor")) {
		ImGui::Dummy({ 0, 0 }); ImGui::SameLine(ImGui::GetContentRegionAvail().x - 5); 
		if (ImGui::SmallButton("X")) {
			p_selected_asset = nullptr;
			goto window_end;
		}

		ImGui::Text(std::format("Name: {}", p_selected_asset->name).c_str()); 
		static bool show_rename_input = false;
		ImGui::SameLine(); 
		if (ImGui::Button("Rename")) {
			show_rename_input = true;
		}

		if (show_rename_input) {
			ImGui::InputText("##name", &p_selected_asset->name);
			if (p_window->input.IsKeyPressed(Key::Enter) || ImGui::IsItemDeactivated()) {
				show_rename_input = false;
				if (files::PathExists(p_selected_asset->filepath)) {
					size_t last_slash_pos = p_selected_asset->filepath.rfind("/");
					std::string new_path = p_selected_asset->filepath.substr(0, last_slash_pos);
					std::string extension = p_selected_asset->filepath.substr(p_selected_asset->filepath.rfind('.') + 1);
					new_path += "/" + AssetLoader::GenAssetFilename(p_selected_asset, extension);

					// Rename and recreate serialized file to reflect asset name update
					files::FileCopy(p_selected_asset->filepath, new_path);
					files::FileDelete(p_selected_asset->filepath);
					p_selected_asset->filepath = new_path;
					SerializeAllAssets();
				}
			}
		}
		

		ImGui::Text(std::format("Filepath: {}", p_selected_asset->filepath).c_str());
		ImGui::ImageButton(GetOrCreateAssetImage(p_selected_asset), {150, 150});

		if (dynamic_cast<MaterialAsset*>(p_selected_asset)) {
			ret |= RenderMaterialEditor();
		}

	}

window_end:

	ImGui::End();
	ImGui::PopID();
	return ret;
}

void AssetEditor::Render() {
	return;

	//render_image.TransitionImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eColorAttachmentOptimal,
	//	vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eColorAttachmentOutput, 
	//	vk::PipelineStageFlagBits::eBottomOfPipe, 0);

	//renderer.RenderScene(render_image);
	//render_image.TransitionImageLayout(vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone,
	//	vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eFragmentShader, 0);
}

bool AssetEditor::RenderMaterialEditor() {
	bool ret = false;
	auto* p_mat = dynamic_cast<MaterialAsset*>(p_selected_asset);

	ret |= ImGui::InputFloat3("Albedo", &p_mat->albedo.r);
	ret |= ImGui::InputFloat("Roughness", &p_mat->roughness);
	ret |= ImGui::InputFloat("Metallic", &p_mat->metallic);
	ret |= ImGui::InputFloat("AO", &p_mat->ao);

	static bool emissive;
	emissive = (uint32_t)p_mat->flags & (uint32_t)MaterialAsset::MaterialFlagBits::EMISSIVE;

	if (ImGui::Checkbox("Emissive", &emissive)) {
		ret = true;
		p_mat->FlipFlag(MaterialAsset::MaterialFlagBits::EMISSIVE);
	}

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

			p_box->imgui_render_cb = [data, p_box, this] {
				static std::string new_mesh_name = "New mesh";
				ImGui::Text("Name: "); ImGui::SameLine(); ImGui::InputText("##NAME", &new_mesh_name);
				if (ImGui::Button("Create")) {
					p_box->close = true;
					auto new_mesh = AssetManager::CreateAsset<StaticMeshAsset>();
					new_mesh->data = *data;
					new_mesh->name = new_mesh_name;
					AssetLoader::SerializeStaticMeshAsset(p_editor->project.directory + "/res/meshes/" + AssetLoader::GenAssetFilename(new_mesh.get(), ".smesh"), *new_mesh.get());
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
		AssetEntry entry{};
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

bool AssetEditor::OnRequestMeshAssetAddFromFile(const std::string& filepath, const std::string& name) {
	bool ret = false;

	if (!filepath.empty()) {
		auto mesh_data = AssetManager::CreateAsset<MeshDataAsset>();
		mesh_data->name = name;
		mesh_data->filepath = filepath;

		if (auto p_data = AssetLoader::LoadMeshDataFromRawFile(filepath)) {
			AssetManager::Get().mesh_buffer_manager.LoadMeshFromData(mesh_data.get(), *p_data);

			// Serialize the mesh as well as any new textures/materials created from it
			for (auto mat_uuid : p_data->materials) {
				if (mat_uuid == AssetManager::CoreAssetIDs::MATERIAL)
					continue;

				auto mat = AssetManager::GetAsset<MaterialAsset>(mat_uuid);
				AssetLoader::SerializeMaterialBinary(p_editor->project.directory + "/res/materials/" + AssetLoader::GenAssetFilename(mat.get(), "mat"), *mat.get());
			}

			for (auto tex_uuid : p_data->textures) {
				auto tex = AssetManager::GetAsset<Texture2DAsset>(tex_uuid);
				// tex->filepath is still the path to the raw image file, will change to new serialized path after this function
				AssetLoader::SerializeTexture2DBinaryFromRawFile(p_editor->project.directory + "/res/textures/" + AssetLoader::GenAssetFilename(tex.get(), "tex2d"), *tex.get(), tex->filepath);
			}

			AssetLoader::SerializeMeshDataBinary(p_editor->project.directory + "/res/meshes/" + AssetLoader::GenAssetFilename(mesh_data.get(), "meshdata"), *p_data, *mesh_data.get());
			ret = true;
		}
		else {
			DeleteAsset(mesh_data.get());
			p_editor->ErrorMessagePopup(std::format("Failed to load mesh file '{}', check logs for more info", filepath));
		}
	}

	return ret;
}

bool AssetEditor::AddAssetButton() {
	static std::unordered_map <std::string, std::function<void()>> popup_options;
	bool ret = false;

	// Any newly created assets are immediately serialized and given a filepath

	popup_options["Mesh data"] = [this, &ret] {
		std::string filepath = files::SelectFileFromExplorer();
		if (filepath.empty())
			return;

		auto* p_box = p_editor->CreateDialogBox();
		p_box->name = "Import mesh";
		p_box->imgui_render_cb = [p_box, filepath, this] {
			static std::string mesh_name_str;
			ImGui::Text("New mesh name: "); ImGui::SameLine();
			ImGuiWidgets::InputAlnumText("##name", mesh_name_str);
			if (ImGui::Button("Set name") && !mesh_name_str.empty()) {
				p_box->close = true;
				OnRequestMeshAssetAddFromFile(filepath, mesh_name_str);
			}
		};
	};

	popup_options["Texture"] = [this, &ret] {
		std::string filepath = files::SelectFileFromExplorer();
		OnRequestTextureAssetAddFromFile(filepath);
		ret = true;
	};

	popup_options["Material"] = [this, &ret] {
		auto mat = AssetManager::CreateAsset<MaterialAsset>();
		AssetLoader::SerializeMaterialBinary(p_editor->project.directory + "/res/materials/" + AssetLoader::GenAssetFilename(mat.get(), "mat"), *mat.get());
		ret = true;
	};

	ImGui::Button("Add asset");
	ImGuiWidgets::Popup("##add asset", popup_options, ImGui::IsItemClicked());

	return ret;
}

void AssetEditor::DeleteAsset(Asset* p_asset) {
	SNK_ASSERT(files::PathExists(p_asset->filepath));
	asset_deletion_queue.push_back(p_asset);

	if (asset_images.contains(p_asset))
		asset_images.erase(p_asset);

	files::FileDelete(p_asset->filepath);
}

void AssetEditor::SerializeAllAssets() {
	// Mesh datas are ignored here as their data will never change so they're just serialized once on load, they're mainly just a bundle of vertex data

	auto textures = AssetManager::GetView<Texture2DAsset>();
	for (auto* p_tex : textures) {
		// p_tex->filepath should already be up to date as it's Texture2DAsset's are serialized immediately on load
		AssetLoader::SerializeTexture2DBinary(p_tex->filepath, *p_tex);
	}

	auto materials = AssetManager::GetView<MaterialAsset>();
	for (auto* p_mat : materials) {
		AssetLoader::SerializeMaterialBinary(p_editor->project.directory + "/res/materials/" + AssetLoader::GenAssetFilename(p_mat, "mat"), *p_mat);
	}

	auto static_meshes = AssetManager::GetView<StaticMeshAsset>();
	for (auto* p_mesh : static_meshes) {
		AssetLoader::SerializeStaticMeshAsset(p_editor->project.directory + "/res/meshes/" + AssetLoader::GenAssetFilename(p_mesh, "smesh"), *p_mesh);
	}
}

void AssetEditor::DeserializeAllAssetsFromActiveProject() {
	for (const auto& entry : std::filesystem::recursive_directory_iterator(p_editor->project.directory + "/res/textures/")) {
		std::string path = entry.path().string();

		if (!path.ends_with(".tex2d"))
			continue;

		AssetLoader::DeserializeTexture2D(path);
	}

	for (const auto& entry : std::filesystem::recursive_directory_iterator(p_editor->project.directory + "/res/materials/")) {
		std::string path = entry.path().string();

		if (!path.ends_with(".mat"))
			continue;

		AssetLoader::DeserializeMaterial(path);
	}

	for (const auto& entry : std::filesystem::recursive_directory_iterator(p_editor->project.directory + "/res/meshes/")) {
		std::string path = entry.path().string();

		if (!path.ends_with(".meshdata"))
			continue;

		AssetLoader::DeserializeMeshData(path);
	}

	for (const auto& entry : std::filesystem::recursive_directory_iterator(p_editor->project.directory + "/res/meshes/")) {
		std::string path = entry.path().string();

		if (!path.ends_with(".smesh"))
			continue;

		AssetLoader::DeserializeStaticMeshAsset(path);
	}
}


void AssetEditor::OnRequestTextureAssetAddFromFile(const std::string& filepath) {
	static vk::Format load_format = vk::Format::eR8G8B8A8Srgb;

	auto* p_box = p_editor->CreateDialogBox();
	p_box->block_other_window_input = true;
	p_box->name = "Texture settings";

	p_box->imgui_render_cb = [filepath, this, p_box] {
		static bool format_is_srgb = false;
		format_is_srgb = load_format == vk::Format::eR8G8B8A8Srgb;
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 25); if (ImGui::Button("X")) p_box->close = true;
		ImGui::Text(std::format("Selected file: '{}'", filepath).c_str());
		if (ImGui::Checkbox("SRGB (select for albedo textures)", &format_is_srgb))
			load_format = format_is_srgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

		if (ImGui::Button("Load")) {
			if (!filepath.empty()) {
				auto tex = AssetManager::CreateAsset<Texture2DAsset>();
				tex->filepath = filepath;
				if (!AssetLoader::LoadTextureFromFile(tex, load_format)) {
					DeleteAsset(tex.get());
					p_editor->ErrorMessagePopup(std::format("Failed to load texture file '{}', check logs for more info", filepath));
				}
				else {
					AssetLoader::SerializeTexture2DBinaryFromRawFile(p_editor->project.directory + "/res/textures/" + AssetLoader::GenAssetFilename(tex.get(), "tex2d"), *tex.get(), filepath);
				}

			}
			p_box->close = true;
		}
	};

}


bool AssetEditor::RenderImGui() {
	if (!asset_deletion_queue.empty()) {
		VkContext::GetLogicalDevice().GraphicsQueueWaitIdle();
	}

	for (auto* p_asset : asset_deletion_queue) {
		AssetManager::DeleteAsset(p_asset);
	}
	asset_deletion_queue.clear();

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
				if (ImGui::Selectable(asset_types[i], &selected, 0, {400.f, (float)ASSET_WINDOW_HEIGHT / asset_types.size()})) {
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
