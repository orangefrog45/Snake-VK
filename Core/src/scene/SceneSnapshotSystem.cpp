#include "scene/SceneSnapshotSystem.h"
#include "scene/TransformBufferSystem.h"
#include "components/Components.h"

using namespace SNAKE;

void SceneSnapshotSystem::OnSystemAdd() {
	m_frame_start_listener.callback = [this]([[maybe_unused]] Event const* p_event) {
		TakeSceneSnapshot();
	};
	EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);
}

void SceneSnapshotSystem::TakeSceneSnapshot() {
	m_snapshot_data.Reset();

	std::vector<StaticMeshComponent*> comps;

	for (auto [entity, mesh] : p_scene->GetRegistry().view<StaticMeshComponent>().each()) {
		comps.push_back(&mesh);
	}

	// Order meshes based on their data
	std::ranges::sort(comps, [](const auto& mesh0, const auto& mesh1) -> bool { return mesh0->GetMeshAsset()->uuid() < mesh1->GetMeshAsset()->uuid(); });

	StaticMeshAsset* p_current_mesh = comps.empty() ? nullptr : comps[0]->GetMeshAsset();
	uint32_t mesh_change_index = 0;
	uint32_t same_mesh_count = 0;

	for (uint32_t i = 0; i < comps.size(); i++) {
		auto* p_mesh = comps[i];

		if (auto* p_new_mesh = p_mesh->GetMeshAsset(); p_new_mesh != p_current_mesh) {
			m_snapshot_data.mesh_ranges.emplace_back(p_current_mesh->uuid(), mesh_change_index, same_mesh_count);
			p_current_mesh = p_new_mesh;
			same_mesh_count = 0;
			mesh_change_index = i;
		}

		same_mesh_count++;
		m_snapshot_data.static_mesh_data.emplace_back(&p_mesh->GetMaterials(), p_mesh->GetEntity()->GetComponent<TransformBufferIdxComponent>()->idx);
	}

	if (p_current_mesh)
		m_snapshot_data.mesh_ranges.emplace_back(p_current_mesh->uuid(), mesh_change_index, same_mesh_count);
}

