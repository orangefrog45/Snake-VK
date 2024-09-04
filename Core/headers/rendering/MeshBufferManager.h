#pragma once
#include "core/S_VkBuffer.h"

namespace SNAKE {
	struct MeshEntryData {
		uint32_t data_start_indices_idx;
		uint32_t data_start_vertex_idx;

		uint32_t num_indices;
		uint32_t num_vertices;
	};

	class MeshBufferManager {
	public:
		void LoadMeshFromData(class MeshDataAsset* p_mesh_data_asset, class MeshData& data);

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
			return { *mp_position_buf.get(), *mp_normal_buf.get() , *mp_tangent_buf.get() , *mp_tex_coord_buf.get() , *mp_index_buf.get() };
		}

	private:
		uint32_t m_total_vertices = 0;
		uint32_t m_total_indices = 0;

		std::unordered_map<MeshDataAsset*, MeshEntryData> m_entries;

		std::unique_ptr<S_VkBuffer> mp_position_buf = nullptr;
		std::unique_ptr<S_VkBuffer> mp_normal_buf = nullptr;
		std::unique_ptr<S_VkBuffer> mp_tex_coord_buf = nullptr;
		std::unique_ptr<S_VkBuffer> mp_tangent_buf = nullptr;
		std::unique_ptr<S_VkBuffer> mp_index_buf = nullptr;
	};

}