#pragma once
#include "System.h"
#include "events/EventManager.h"
#include "assets/MaterialAsset.h"

namespace SNAKE {
	struct SceneSnapshotData {
		struct MeshRange {
			MeshRange(uint64_t uuid, uint32_t _start_idx, uint32_t _count) : mesh_uuid(uuid), start_idx(_start_idx), count(_count) {};
			uint64_t mesh_uuid;
			uint32_t start_idx;
			uint32_t count;
		};

		struct MeshRenderData {
			MeshRenderData(const std::vector<AssetRef<MaterialAsset>>& m, uint32_t _transform_buffer_idx) : material_vec(m), transform_buffer_idx(_transform_buffer_idx) {}
			// A copy of material pointers for this mesh
			std::vector<AssetRef<MaterialAsset>> material_vec;
			uint32_t transform_buffer_idx;
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

	class SceneSnapshotSystem : public System {
	public:
		void OnSystemAdd() override;

		const SceneSnapshotData& GetSnapshotData() const {
			return m_snapshot_data;
		}
	private:
		void TakeSceneSnapshot();

		SceneSnapshotData m_snapshot_data;

		EventListener m_frame_start_listener;
	};
}