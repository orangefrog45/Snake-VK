#include "pch/pch.h"
#include "assets/AssetManager.h"
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

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			descriptor_buffers[i] = std::make_shared<DescriptorBuffer>();

			auto p_descriptor_spec = std::make_shared<DescriptorSetSpec>();
			descriptor_buffers[i]->SetDescriptorSpec(p_descriptor_spec);

			p_descriptor_spec->AddDescriptor(0, vk::DescriptorType::eStorageBuffer,
				vk::ShaderStageFlagBits::eAllGraphics, 1)
				.AddDescriptor(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAllGraphics, 4096);

			p_descriptor_spec->GenDescriptorLayout();

			descriptor_buffers[i]->CreateBuffer(1);
		}

		m_global_material_buffer_manager.Init(descriptor_buffers);
		m_global_tex_buffer_manager.Init(descriptor_buffers);
	}

	void AssetManager::LoadCoreAssets() {
		auto tex = CreateAsset<Texture2DAsset>(CoreAssetIDs::TEXTURE);
		tex->LoadFromFile("res/textures/metalgrid1_basecolor.png", vk::Format::eR8G8B8A8Srgb);

		auto normal_tex = CreateAsset<Texture2DAsset>();
		normal_tex->name = "NORMAL MAP";
		normal_tex->LoadFromFile("res/textures/metalgrid1_normal-ogl.png", vk::Format::eR8G8B8A8Unorm);

		auto mesh = CreateAsset<StaticMeshAsset>(CoreAssetIDs::SPHERE_MESH);
		mesh->data = CreateAsset<MeshDataAsset>();
		mesh->data->filepath = "res/meshes/sphere.glb";
		LoadMeshFromFile(mesh->data);

		auto material = CreateAsset<MaterialAsset>(CoreAssetIDs::MATERIAL);
		material->albedo_tex = tex;
		material->normal_tex = normal_tex;
		material->name = "Default material";
		material->DispatchUpdateEvent();

	}

	void AssetManager::Shutdown() {
		auto& assets = Get().m_assets;

		while (!assets.empty()) {
			DeleteAsset(assets.begin()->second);
			assets.erase(assets.begin());
		}
	}

	void AssetManager::DeleteAsset(Asset* asset) {
		if (!Get().m_assets.contains(asset->uuid())) {
			SNK_CORE_ERROR("AssetManager::DeleteAsset failed '[{}, {}]', UUID doesn't exist in AssetManager", asset->uuid(), asset->filepath);
			return;
		}

		if (auto ref_count = asset->GetRefCount(); ref_count != 0) {
			SNK_CORE_WARN("Deleting asset '[{}, {}]' which still has {} references", asset->uuid(), asset->filepath, ref_count);
		}

		Get().m_assets.erase(asset->uuid());
		delete asset;
	}

	std::pair<AssetRef<Texture2DAsset>, bool>  AssetManager::CreateOrGetTextureFromMaterial(const std::string& dir, aiTextureType type, aiMaterial* p_material) {
		AssetRef<Texture2DAsset> tex = nullptr;

		if (p_material->GetTextureCount(type) > 0) {
			aiString path;

			if (p_material->GetTexture(type, 0, &path, NULL, NULL, NULL, NULL, NULL) == aiReturn_SUCCESS) {
				std::string p(path.data);
				std::string full_path;

				if (p.starts_with(".\\"))
					p = p.substr(2, p.size() - 2);

				full_path = dir + "\\" + p;

				if (auto existing = GetAsset<Texture2DAsset>(full_path)) {
					return { existing, false };
				}

				if (!files::PathExists(full_path)) {
					SNK_CORE_ERROR("CreateOrGetTextureFromMaterial failed, invalid path '{}'", full_path);
					return { nullptr, false };
				}

				tex = CreateAsset<Texture2DAsset>();
				vk::Format fmt = type == aiTextureType_BASE_COLOR ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
				tex->LoadFromFile(full_path, fmt);
			}
		}

		return { tex, true };
	}

	bool AssetManager::LoadTextureFromFile(AssetRef<Texture2DAsset> tex, vk::Format fmt) {
		int width, height, channels;
		// Force 4 channels as most GPUs only support these as samplers
		stbi_uc* pixels = stbi_load(tex->filepath.c_str(), &width, &height, &channels, 4);

		if (!pixels) {
			return false;
		}
		vk::DeviceSize image_size = width * height * 4;

		S_VkBuffer staging_buffer{};
		staging_buffer.CreateBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
		void* p_data = staging_buffer.Map();
		memcpy(p_data, pixels, (size_t)image_size);
		staging_buffer.Unmap();

		stbi_image_free(pixels);

		Image2DSpec spec;
		spec.format = fmt;
		spec.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc;
		spec.size.x = width;
		spec.size.y = height;
		spec.tiling = vk::ImageTiling::eOptimal;
		spec.aspect_flags = vk::ImageAspectFlagBits::eColor;
		spec.mip_levels = static_cast<unsigned>(glm::floor(glm::log2(glm::max((float)spec.size.x, (float)spec.size.y)))) + 1;

		tex->image.SetSpec(spec);
		tex->image.CreateImage();

		tex->image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 0, spec.mip_levels);
		CopyBufferToImage(staging_buffer.buffer, tex->image.GetImage(), width, height);
		tex->image.GenerateMipmaps(vk::ImageLayout::eTransferDstOptimal);
		tex->image.TransitionImageLayout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 0, spec.mip_levels);

		tex->image.CreateImageView();
		tex->image.CreateSampler();

		Get().m_global_tex_buffer_manager.RegisterTexture(tex);

		return true;
	}

	std::unique_ptr<AssetManager::MeshData> AssetManager::LoadMeshFromFile(AssetRef<MeshDataAsset> mesh_data_asset) {
		auto data = std::make_unique<MeshData>();

		const aiScene* p_scene = data->importer.ReadFile(mesh_data_asset->filepath, 
			aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_CalcTangentSpace);

		if (!p_scene) {
			SNK_CORE_ERROR("Failed to import mesh from filepath '{}' : '{}'", mesh_data_asset->filepath, data->importer.GetErrorString());
			return nullptr;
		}

		unsigned num_vertices = 0;
		unsigned num_indices = 0;

		for (size_t i = 0; i < p_scene->mNumMeshes; i++) {
			auto* p_submesh = p_scene->mMeshes[i];

			Submesh sm;
			sm.base_index = num_indices;
			sm.base_vertex = num_vertices;
			sm.num_indices = p_submesh->mNumFaces * 3;
			sm.material_index = p_submesh->mMaterialIndex;
			mesh_data_asset->submeshes.push_back(sm);

			num_vertices += p_submesh->mNumVertices;
			num_indices += p_submesh->mNumFaces * 3;
		}

		mesh_data_asset->position_buf.CreateBuffer(num_vertices * sizeof(aiVector3D),
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

		mesh_data_asset->normal_buf.CreateBuffer(num_vertices * sizeof(aiVector3D),
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

		mesh_data_asset->index_buf.CreateBuffer(num_indices * sizeof(uint32_t),
			vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);

		mesh_data_asset->tex_coord_buf.CreateBuffer(num_vertices * sizeof(aiVector2D),
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

		mesh_data_asset->tangent_buf.CreateBuffer(num_vertices * sizeof(aiVector3D),
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

		S_VkBuffer staging_buf_pos;
		staging_buf_pos.CreateBuffer(num_vertices * sizeof(aiVector3D),
			vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

		S_VkBuffer staging_buf_norm;
		staging_buf_norm.CreateBuffer(num_vertices * sizeof(aiVector3D),
			vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

		S_VkBuffer staging_buf_index;
		staging_buf_index.CreateBuffer(num_indices * sizeof(unsigned),
			vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

		S_VkBuffer staging_buf_tex_coord;
		staging_buf_tex_coord.CreateBuffer(num_vertices * sizeof(aiVector2D),
			vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

		S_VkBuffer staging_buf_tangent;
		staging_buf_tangent.CreateBuffer(num_vertices * sizeof(aiVector3D),
			vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

		std::byte* p_data_pos = reinterpret_cast<std::byte*>(staging_buf_pos.Map());
		std::byte* p_data_norm = reinterpret_cast<std::byte*>(staging_buf_norm.Map());
		std::byte* p_data_index = reinterpret_cast<std::byte*>(staging_buf_index.Map());
		std::byte* p_data_tex_coord = reinterpret_cast<std::byte*>(staging_buf_tex_coord.Map());
		std::byte* p_data_tangent = reinterpret_cast<std::byte*>(staging_buf_tangent.Map());

		size_t current_vert_offset = 0;

		for (size_t i = 0; i < p_scene->mNumMeshes; i++) {
			auto& submesh = *p_scene->mMeshes[i];
			memcpy(p_data_pos + current_vert_offset * sizeof(aiVector3D), submesh.mVertices, submesh.mNumVertices * sizeof(aiVector3D));
			memcpy(p_data_norm + current_vert_offset * sizeof(aiVector3D), submesh.mNormals, submesh.mNumVertices * sizeof(aiVector3D));
			memcpy(p_data_tangent + current_vert_offset * sizeof(aiVector3D), submesh.mTangents, submesh.mNumVertices * sizeof(aiVector3D));
			current_vert_offset += submesh.mNumVertices;
		}

		data->indices.resize(num_indices);
		data->tex_coords.resize(num_vertices);

		unsigned indices_offset = 0;

		for (size_t i = 0; i < p_scene->mNumMeshes; i++) {
			aiMesh& submesh = *p_scene->mMeshes[i];
			for (size_t j = 0; j < submesh.mNumFaces; j++) {
				aiFace& face = submesh.mFaces[j];
				memcpy(data->indices.data() + indices_offset, face.mIndices, sizeof(unsigned) * 3);
				indices_offset += 3;
			}

			for (size_t j = 0; j < submesh.mNumVertices; j++) {
				memcpy(data->tex_coords.data() + j, &submesh.mTextureCoords[0][j], sizeof(aiVector2D));
			}
		}

		memcpy(p_data_index, data->indices.data(), data->indices.size() * sizeof(unsigned));
		memcpy(p_data_tex_coord, data->tex_coords.data(), data->tex_coords.size() * sizeof(aiVector2D));

		CopyBuffer(staging_buf_pos.buffer, mesh_data_asset->position_buf.buffer, num_vertices * sizeof(aiVector3D));
		CopyBuffer(staging_buf_norm.buffer, mesh_data_asset->normal_buf.buffer, num_vertices * sizeof(aiVector3D));
		CopyBuffer(staging_buf_index.buffer, mesh_data_asset->index_buf.buffer, num_indices * sizeof(unsigned));
		CopyBuffer(staging_buf_tex_coord.buffer, mesh_data_asset->tex_coord_buf.buffer, num_vertices * sizeof(aiVector2D));
		CopyBuffer(staging_buf_tangent.buffer, mesh_data_asset->tangent_buf.buffer, num_vertices * sizeof(aiVector3D));

		staging_buf_pos.Unmap();
		staging_buf_index.Unmap();
		staging_buf_norm.Unmap();
		staging_buf_tex_coord.Unmap();
		staging_buf_tangent.Unmap();

		mesh_data_asset->materials.resize(p_scene->mNumMaterials, nullptr);

		if (mesh_data_asset->materials.empty()) {
			mesh_data_asset->materials.push_back(GetAsset<MaterialAsset>(CoreAssetIDs::MATERIAL));
		}

		auto dir = files::GetFileDirectory(mesh_data_asset->filepath);

		for (size_t i = 0; i < p_scene->mNumMaterials; i++) {
			aiMaterial* p_material = p_scene->mMaterials[i];
			AssetRef<MaterialAsset> material_asset = CreateAsset<MaterialAsset>();

			bool material_properties_set = false;

			if (auto [tex, is_newly_created] = CreateOrGetTextureFromMaterial(dir, aiTextureType_BASE_COLOR, p_material); tex) {
				material_asset->albedo_tex = tex;
				if (is_newly_created) data->textures.push_back(tex);
			}
			if (auto [tex, is_newly_created] = CreateOrGetTextureFromMaterial(dir, aiTextureType_NORMALS, p_material); tex) {
				material_asset->normal_tex = tex;
				if (is_newly_created) data->textures.push_back(tex);
			}
			if (auto [tex, is_newly_created] = CreateOrGetTextureFromMaterial(dir, aiTextureType_DIFFUSE_ROUGHNESS, p_material); tex) {
				material_asset->roughness_tex = tex;
				if (is_newly_created) data->textures.push_back(tex);
			}
			if (auto [tex, is_newly_created] = CreateOrGetTextureFromMaterial(dir, aiTextureType_METALNESS, p_material); tex) {
				material_asset->metallic_tex = tex;
				if (is_newly_created) data->textures.push_back(tex);
			}
			if (auto [tex, is_newly_created] = CreateOrGetTextureFromMaterial(dir, aiTextureType_AMBIENT_OCCLUSION, p_material); tex) {
				material_asset->ao_tex = tex;
				if (is_newly_created) data->textures.push_back(tex);
			}


			aiColor3D base_col(1.f, 1.f, 1.f);
			if (p_material->Get(AI_MATKEY_BASE_COLOR, base_col) == aiReturn_SUCCESS) {
				material_asset->albedo = { base_col.r, base_col.g, base_col.b };
				material_properties_set = true;
			}

			float roughness;
			float metallic;
			if (p_material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == aiReturn_SUCCESS) {
				material_asset->roughness = roughness;
				material_properties_set = true;
			}

			if (p_material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == aiReturn_SUCCESS) {
				material_asset->metallic = metallic;
				material_properties_set = true;
			}

			if (material_properties_set) {
				data->materials.push_back(material_asset);
			}
			else {
				material_asset = GetAsset<MaterialAsset>(CoreAssetIDs::MATERIAL);
				AssetManager::DeleteAsset(material_asset->uuid());
			}

			mesh_data_asset->materials[i] = material_asset;
		}

		return std::move(data);
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

		delete assets[uuid];
	}

	
}