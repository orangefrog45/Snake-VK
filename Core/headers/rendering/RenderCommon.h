#pragma once
#include "entt/entt.hpp"
#include "components/Component.h"

namespace SNAKE {
	struct SceneSnapshotData {
		struct MeshRange {
			MeshRange(uint64_t uuid, uint32_t _start_idx, uint32_t _count) : mesh_uuid(uuid), start_idx(_start_idx), count(_count) {};
			uint64_t mesh_uuid;
			uint32_t start_idx;
			uint32_t count;
		};

		struct MeshRenderData {
			MeshRenderData(std::vector<AssetRef<MaterialAsset>> const* m) : material_vec(m) {}
			std::vector<AssetRef<MaterialAsset>> const* material_vec;
		};

		void Reset() {
			size_t prev_range_size = mesh_ranges.size();
			size_t prev_sm_size = static_mesh_data.size();

			mesh_ranges.clear();
			mesh_ranges.reserve(prev_range_size);
			static_mesh_data.clear();
			static_mesh_data.reserve(prev_sm_size);
		}

		std::vector<MeshRange> mesh_ranges;

		// Vector sorted into groups of transforms for each mesh
		std::vector<MeshRenderData> static_mesh_data;

	};
}