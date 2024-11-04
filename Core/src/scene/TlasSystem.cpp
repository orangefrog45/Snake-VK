#include "pch/pch.h"
#include "components/Components.h"
#include "scene/Scene.h"
#include "scene/TlasSystem.h"

using namespace SNAKE;

void TlasSystem::OnSystemAdd() {
	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_cmd_buffers[i].Init(vk::CommandBufferLevel::ePrimary);
		BuildTlas(i);
		VkContext::GetLogicalDevice().GraphicsQueueWaitIdle();

	}

	m_frame_start_listener.callback = [&]([[maybe_unused]] Event const* p_event) {
		BuildTlas(VkContext::GetCurrentFIF());
	};

	EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);
}

void TlasSystem::OnSystemRemove() {
	EventManagerG::DeregisterListener(m_frame_start_listener);
}

void TlasSystem::BuildTlas(FrameInFlightIndex idx) {
	if (m_tlas_array[idx].GetAS()) {
		m_tlas_array[idx].Destroy();
	}

	std::vector<vk::AccelerationStructureInstanceKHR> instances;

	auto& reg = p_scene->GetRegistry();
	auto mesh_view = reg.view<StaticMeshComponent>();

	for (auto [entity, mesh] : mesh_view.each()) {
		for (auto& blas : mesh.GetMeshAsset()->data->submesh_blas_array) {
			instances.push_back(blas->GenerateInstance(reg.get<TransformComponent>(entity)));
		}
	}

	auto cmd = *m_cmd_buffers[VkContext::GetCurrentFIF()].buf;

	m_tlas_array[idx].BuildFromInstances(instances, cmd);



}