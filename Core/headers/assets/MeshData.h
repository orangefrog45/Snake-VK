#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "util/Logger.h"
#include "resources/S_VkBuffer.h"
#include "core/VkCommon.h"
#include "assets/MaterialAsset.h"

namespace SNAKE {
	struct Submesh {
		Submesh() : num_indices(0), base_vertex(0), base_index(0), material_index(0) {};

		unsigned int num_indices;
		unsigned int num_vertices;
		unsigned int base_vertex;
		unsigned int base_index;
		unsigned int material_index;
	};

	struct MeshData {
		MeshData() = default;
		~MeshData() {
			delete[] positions;
			delete[] normals;
			delete[] tangents;
			delete[] tex_coords;
			delete[] indices;
		}
		// Textures (UUIDS) loaded from the mesh
		std::vector<uint64_t> textures;

		// Materials (UUIDS) loaded from the mesh
		std::vector<uint64_t> materials;

		std::vector<Submesh> submeshes;

		unsigned num_indices = 0;
		unsigned num_vertices = 0;

		aiVector3D* positions = nullptr;
		aiVector3D* normals = nullptr;
		aiVector3D* tangents = nullptr;
		aiVector2D* tex_coords = nullptr;
		unsigned* indices = nullptr;
	};

	struct MeshDataAsset : public Asset {
		MeshDataAsset(uint64_t uuid = 0) : Asset(uuid) {};

		// Materials that were loaded with the mesh - if deleted these are reset to the default material asset
		std::vector<AssetRef<MaterialAsset>> materials;

		std::vector<Submesh> submeshes;
		unsigned num_indices = 0;
		unsigned num_vertices = 0;

		// Memory managed by MeshBufferManager
		std::vector<class BLAS*> submesh_blas_array;
	};

	class StaticMeshAsset : public Asset {
	public:
		AssetRef<MeshDataAsset> data{ nullptr };
	private:
		StaticMeshAsset(uint64_t uuid) : Asset(uuid) {};
		friend class AssetManager;
	};
}