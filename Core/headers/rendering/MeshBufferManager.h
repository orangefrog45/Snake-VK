#pragma once
#include "resources/S_VkBuffer.h"

namespace SNAKE {
	struct MeshEntryData {
		uint32_t data_start_indices_idx;
		uint32_t data_start_vertex_idx;

		uint32_t num_indices;
		uint32_t num_vertices;
	};

	class MeshBufferManager {
	public:
		void Init();

		void LoadMeshFromData(struct MeshDataAsset* p_mesh_data_asset, struct MeshData& data);

		void UnloadMesh(MeshDataAsset* p_mesh_data_asset);

		const MeshEntryData& GetEntryData(MeshDataAsset* p_mesh_data_asset) const {
			SNK_ASSERT(m_entries.contains(p_mesh_data_asset));
			return m_entries.at(p_mesh_data_asset);
		}

		struct MeshBuffers {
			MeshBuffers(S_VkBuffer& pos_buf, S_VkBuffer& norm_buf, S_VkBuffer& tan_buf, S_VkBuffer& tex_buf, S_VkBuffer& idx_buf) :
				position_buf(pos_buf), normal_buf(norm_buf), tangent_buf(tan_buf), tex_coord_buf(tex_buf), indices_buf(idx_buf) {}

			S_VkBuffer& position_buf;
			S_VkBuffer& normal_buf;
			S_VkBuffer& tangent_buf;
			S_VkBuffer& tex_coord_buf;
			S_VkBuffer& indices_buf;
		};

		MeshBuffers GetMeshBuffers() {
			return { m_position_buf, m_normal_buf , m_tangent_buf , m_tex_coord_buf , m_index_buf };
		}

	private:
		uint32_t m_total_vertices = 0;
		uint32_t m_total_indices = 0;

		std::unordered_map<MeshDataAsset*, MeshEntryData> m_entries;

		S_VkBuffer m_position_buf;
		S_VkBuffer m_normal_buf;
		S_VkBuffer m_tex_coord_buf;
		S_VkBuffer m_tangent_buf;
		S_VkBuffer m_index_buf;
	};

}