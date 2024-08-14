#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "util/Logger.h"
#include "core/S_VkBuffer.h"
#include "core/VkCommon.h"
#include "Asset.h"

namespace SNAKE {

	struct StaticMeshDataAsset : public Asset {
		StaticMeshDataAsset(uint64_t uuid = 0) : Asset(uuid) {};

		bool ImportFile(const std::string& t_filepath) {
			Assimp::Importer importer;

			const aiScene* p_scene = importer.ReadFile(t_filepath, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);

			if (!p_scene) {
				SNK_CORE_ERROR("Failed to import mesh from filepath '{}' : '{}'", t_filepath, importer.GetErrorString());
				return false;
			}

			unsigned num_vertices = 0;
			unsigned num_indices = 0;

			for (size_t i = 0; i < p_scene->mNumMeshes; i++) {
				auto* p_submesh = p_scene->mMeshes[i];
				num_vertices += p_submesh->mNumVertices;
				num_indices += p_submesh->mNumFaces * 3;
			}

			position_buf.CreateBuffer(num_vertices * sizeof(aiVector3D), 
				vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

			normal_buf.CreateBuffer(num_vertices * sizeof(aiVector3D),
				vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);

			index_buf.CreateBuffer(num_indices * sizeof(uint32_t),
				vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);

			tex_coord_buf.CreateBuffer(num_vertices * sizeof(aiVector2D),
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

			CopyBuffer(staging_buf_pos.buffer, position_buf.buffer, num_vertices * sizeof(aiVector3D));
			CopyBuffer(staging_buf_norm.buffer, normal_buf.buffer, num_vertices * sizeof(aiVector3D));
			CopyBuffer(staging_buf_index.buffer, index_buf.buffer, num_indices * sizeof(unsigned));
			CopyBuffer(staging_buf_tex_coord.buffer, tex_coord_buf.buffer, num_vertices * sizeof(aiVector2D));

			staging_buf_pos.Unmap();
			staging_buf_index.Unmap();
			staging_buf_norm.Unmap();
			staging_buf_tex_coord.Unmap();

			return true;
		}

		S_VkBuffer position_buf;
		S_VkBuffer normal_buf;
		S_VkBuffer index_buf;
		S_VkBuffer tex_coord_buf;
	};

	class StaticMeshAsset : public Asset {
	public:
		std::shared_ptr<StaticMeshDataAsset> data;
	private:
		StaticMeshAsset(uint64_t uuid) : Asset(uuid) {};
		friend class AssetManager;
	};
}