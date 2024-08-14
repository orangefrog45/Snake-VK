#include "pch/pch.h"
#include "assets/AssetManager.h"
#include "util/util.h"
#include "assets/MeshData.h"

namespace SNAKE {

	void AssetManager::I_Init(vk::CommandPool pool) {
		InitGlobalBufferManagers();
		LoadCoreAssets(pool);
	}

	void AssetManager::InitGlobalBufferManagers() {
		std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT> descriptor_buffers;

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			descriptor_buffers[i] = std::make_shared<DescriptorBuffer>();

			auto p_descriptor_spec = std::make_shared<DescriptorSetSpec>();
			descriptor_buffers[i]->SetDescriptorSpec(p_descriptor_spec);

			p_descriptor_spec->AddDescriptor(0, vk::DescriptorType::eUniformBuffer,
				vk::ShaderStageFlagBits::eAllGraphics, 64)
				.AddDescriptor(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAllGraphics, 4096);

			p_descriptor_spec->GenDescriptorLayout();

			descriptor_buffers[i]->CreateBuffer(1);
		}

		m_global_material_buffer_manager.Init(descriptor_buffers);
		m_global_tex_buffer_manager.Init(descriptor_buffers);
	}

	void AssetManager::LoadCoreAssets(vk::CommandPool pool) {
		auto mesh = CreateAsset<StaticMeshAsset>(CoreAssetIDs::SPHERE_MESH);
		mesh->data = std::make_shared<StaticMeshDataAsset>();
		mesh->data->ImportFile("res/meshes/sphere.glb");
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

		delete Get().m_assets[asset->uuid()];
	}


	bool AssetManager::LoadMeshFromFile(AssetRef<StaticMeshDataAsset> mesh_data_asset) {
		Assimp::Importer importer;

		const aiScene* p_scene = importer.ReadFile(mesh_data_asset->filepath, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);

		if (!p_scene) {
			SNK_CORE_ERROR("Failed to import mesh from filepath '{}' : '{}'", mesh_data_asset->filepath, importer.GetErrorString());
			return false;
		}

		unsigned num_vertices = 0;
		unsigned num_indices = 0;

		for (size_t i = 0; i < p_scene->mNumMeshes; i++) {
			auto* p_submesh = p_scene->mMeshes[i];
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

		std::byte* p_data_pos = reinterpret_cast<std::byte*>(staging_buf_pos.Map());
		std::byte* p_data_norm = reinterpret_cast<std::byte*>(staging_buf_norm.Map());
		std::byte* p_data_index = reinterpret_cast<std::byte*>(staging_buf_index.Map());
		std::byte* p_data_tex_coord = reinterpret_cast<std::byte*>(staging_buf_tex_coord.Map());

		size_t current_vert_offset = 0;

		for (size_t i = 0; i < p_scene->mNumMeshes; i++) {
			auto& submesh = *p_scene->mMeshes[i];
			memcpy(p_data_pos + current_vert_offset * sizeof(aiVector3D), submesh.mVertices, submesh.mNumVertices * sizeof(aiVector3D));
			memcpy(p_data_norm + current_vert_offset * sizeof(aiVector3D), submesh.mNormals, submesh.mNumVertices * sizeof(aiVector3D));
			current_vert_offset += submesh.mNumVertices;
		}

		std::vector<unsigned> indices;
		std::vector<aiVector2D> tex_coords;
		indices.resize(num_indices);
		tex_coords.resize(num_vertices);

		unsigned indices_offset = 0;

		for (size_t i = 0; i < p_scene->mNumMeshes; i++) {
			aiMesh& submesh = *p_scene->mMeshes[i];
			for (size_t j = 0; j < submesh.mNumFaces; j++) {
				aiFace& face = submesh.mFaces[j];
				memcpy(indices.data() + indices_offset, face.mIndices, sizeof(unsigned) * 3);
				indices_offset += 3;
			}

			for (size_t j = 0; j < submesh.mNumVertices; j++) {
				memcpy(tex_coords.data() + j, &submesh.mTextureCoords[0][j], sizeof(aiVector2D));
			}
		}

		memcpy(p_data_index, indices.data(), indices.size() * sizeof(unsigned));
		memcpy(p_data_tex_coord, tex_coords.data(), tex_coords.size() * sizeof(aiVector2D));

		CopyBuffer(staging_buf_pos.buffer, mesh_data_asset->position_buf.buffer, num_vertices * sizeof(aiVector3D));
		CopyBuffer(staging_buf_norm.buffer, mesh_data_asset->normal_buf.buffer, num_vertices * sizeof(aiVector3D));
		CopyBuffer(staging_buf_index.buffer, mesh_data_asset->index_buf.buffer, num_indices * sizeof(unsigned));
		CopyBuffer(staging_buf_tex_coord.buffer, mesh_data_asset->tex_coord_buf.buffer, num_vertices * sizeof(aiVector2D));

		staging_buf_pos.Unmap();
		staging_buf_index.Unmap();
		staging_buf_norm.Unmap();
		staging_buf_tex_coord.Unmap();

		return true;
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