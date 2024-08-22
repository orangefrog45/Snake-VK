#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "util/Logger.h"
#include "core/S_VkBuffer.h"
#include "core/VkCommon.h"
#include "assets/MaterialAsset.h"

namespace SNAKE {
	struct Submesh {
		Submesh() : num_indices(0), base_vertex(0), base_index(0), material_index(0) {};

		unsigned int num_indices;
		unsigned int base_vertex;
		unsigned int base_index;
		unsigned int material_index;
	};

	struct MeshDataAsset : public Asset {
		MeshDataAsset(uint64_t uuid = 0) : Asset(uuid) {};

		// Materials that were loaded with the mesh - if deleted these are reset to the default material asset
		std::vector<AssetRef<MaterialAsset>> materials;

		std::vector<Submesh> submeshes;

		S_VkBuffer position_buf;
		S_VkBuffer normal_buf;
		S_VkBuffer tex_coord_buf;
		S_VkBuffer tangent_buf;
		S_VkBuffer index_buf;
	};

	class StaticMeshAsset : public Asset {
	public:
		AssetRef<MeshDataAsset> data{ nullptr };
	private:
		StaticMeshAsset(uint64_t uuid) : Asset(uuid) {};
		friend class AssetManager;
	};
}