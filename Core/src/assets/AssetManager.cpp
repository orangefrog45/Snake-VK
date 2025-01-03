#include "pch/pch.h"
#include "assets/AssetManager.h"
#include "assets/AssetLoader.h"
#include "util/util.h"
#include "assets/MeshData.h"
#include "util/FileUtil.h"
#include <stb_image.h>

namespace SNAKE {
	void AssetManager::I_Init() {
		InitGlobalBufferManagers();
		LoadCoreAssets();
	}

	void AssetManager::InitGlobalBufferManagers() {
		std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT> descriptor_buffers;
		m_global_tex_mat_descriptor_spec = std::make_shared<DescriptorSetSpec>();
		m_global_tex_mat_descriptor_spec->AddDescriptor(0, vk::DescriptorType::eStorageBuffer,
			vk::ShaderStageFlagBits::eAll, 1)
			.AddDescriptor(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAll, 4096);
		m_global_tex_mat_descriptor_spec->GenDescriptorLayout();

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			descriptor_buffers[i] = std::make_shared<DescriptorBuffer>();
			descriptor_buffers[i]->SetDescriptorSpec(m_global_tex_mat_descriptor_spec);
			descriptor_buffers[i]->CreateBuffer(1);
		}

		m_global_material_buffer_manager.Init(descriptor_buffers);
		m_global_tex_buffer_manager.Init(descriptor_buffers);
		mesh_buffer_manager.Init();
	}

	void AssetManager::LoadCoreAssets() {
		auto tex = CreateAsset<Texture2DAsset>(CoreAssetIDs::TEXTURE);
		tex->filepath = "res/textures/metalgrid1_basecolor.png";
		AssetLoader::LoadTextureFromFile(tex, vk::Format::eR8G8B8A8Srgb);

		//auto normal_tex = CreateAsset<Texture2DAsset>();
		//normal_tex->name = "NORMAL MAP";
		//normal_tex->filepath = "res/textures/metalgrid1_normal-ogl.png";
		//AssetLoader::LoadTextureFromFile(normal_tex, vk::Format::eR8G8B8A8Unorm);

		auto material = CreateAsset<MaterialAsset>(CoreAssetIDs::MATERIAL);
		material->albedo_tex = tex;
		//material->normal_tex = normal_tex;
		material->name = "Default material";
		material->DispatchUpdateEvent();


		auto sphere_mesh = CreateAsset<StaticMeshAsset>(CoreAssetIDs::SPHERE_MESH);
		sphere_mesh->data = CreateAsset<MeshDataAsset>(CoreAssetIDs::SPHERE_MESH_DATA);
		sphere_mesh->data->filepath = "res/meshes/sphere.glb";
		auto p_sphere_data = AssetLoader::LoadMeshDataFromRawFile("res/meshes/sphere.glb", false);
		AssetManager::Get().mesh_buffer_manager.LoadMeshFromData(sphere_mesh->data.get(), *p_sphere_data);
		sphere_mesh->data->materials.push_back(material);

		auto cube_mesh = CreateAsset<StaticMeshAsset>(CoreAssetIDs::CUBE_MESH);
		cube_mesh->data = CreateAsset<MeshDataAsset>(CoreAssetIDs::CUBE_MESH_DATA);
		cube_mesh->data->filepath = "res/meshes/cube.glb";
		auto p_cube_data = AssetLoader::LoadMeshDataFromRawFile("res/meshes/cube.glb", false);
		AssetManager::Get().mesh_buffer_manager.LoadMeshFromData(cube_mesh->data.get(), *p_cube_data);
		cube_mesh->data->materials.push_back(material);

	}

	void AssetManager::Shutdown() {
		auto& assets = Get().m_assets;

		while (!assets.empty()) {
			DeleteAsset(assets.begin()->second);
		}
	}

	void AssetManager::DeleteAsset(Asset* p_asset) {
		if (!Get().m_assets.contains(p_asset->uuid())) {
			SNK_CORE_ERROR("AssetManager::DeleteAsset failed '[{}, {}]', UUID doesn't exist in AssetManager", p_asset->uuid(), p_asset->filepath);
			return;
		}

		if (auto ref_count = p_asset->GetRefCount(); ref_count != 0) {
			SNK_CORE_WARN("Deleting asset '[{}, {}]' which still has {} references", p_asset->uuid(), p_asset->filepath, ref_count);
		}

		Get().OnAssetDelete(p_asset);

		Get().m_assets.erase(p_asset->uuid());
		delete p_asset;
	}


	void AssetManager::DeleteAsset(uint64_t uuid) {
		auto& assets = Get().m_assets;

		if (!assets.contains(uuid)) {
			SNK_CORE_ERROR("AssetManager::DeleteAsset failed, UUID '{}' doesn't exist in AssetManager", uuid);
			return;
		}

		auto* p_asset = assets[uuid];

		if (auto ref_count = p_asset->GetRefCount(); ref_count != 0) {
			SNK_CORE_WARN("Deleting asset '[{}, {}]' which still has {} references", p_asset->uuid(), p_asset->filepath, ref_count);
		}

		Get().OnAssetDelete(p_asset);

		delete assets[uuid];
		assets.erase(uuid);
	}

	
}