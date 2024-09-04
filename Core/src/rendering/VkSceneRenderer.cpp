#include "rendering/VkSceneRenderer.h"
#include "scene/Scene.h"
#include "components/Components.h"
#include "core/JobSystem.h"

using namespace SNAKE;

void VkSceneRenderer::TakeGameStateSnapshot() {
	m_snapshot_data.Reset();

	std::vector<StaticMeshComponent*> comps;

	for (auto [entity, mesh] : mp_scene->GetRegistry().view<StaticMeshComponent>().each()) {
		comps.push_back(&mesh);
	}

	// Order meshes based on their data
	std::ranges::sort(comps, [](const auto& mesh0, const auto& mesh1) -> bool { return mesh0->mesh_asset->uuid() < mesh1->mesh_asset->uuid(); });

	StaticMeshAsset* p_current_mesh = comps.empty() ? nullptr : comps[0]->mesh_asset.get();
	uint32_t mesh_change_index = 0;
	uint32_t same_mesh_count = 0;

	for (uint32_t i = 0; i < comps.size(); i++) {
		auto* p_mesh = comps[i];

		if (auto* p_new_mesh = p_mesh->mesh_asset.get(); p_new_mesh != p_current_mesh) {
			m_snapshot_data.mesh_ranges.emplace_back(p_current_mesh->uuid(), mesh_change_index, same_mesh_count);
			p_current_mesh = p_new_mesh;
			same_mesh_count = 0;
			mesh_change_index = i;
		}

		same_mesh_count++;
		// TODO: reduce jumping through memory
		auto* p_ent = p_mesh->GetEntity();
		m_snapshot_data.static_mesh_data.emplace_back(&p_mesh->materials);
	}

	if (p_current_mesh)
		m_snapshot_data.mesh_ranges.emplace_back(p_current_mesh->uuid(), mesh_change_index, same_mesh_count);
}

void VkSceneRenderer::Init(Window& window, Scene* p_scene) {
	mp_scene = p_scene;

	m_shadow_pass.Init();
	m_forward_pass.Init(&window, &m_shadow_pass.m_dir_light_shadow_map);

	m_frame_start_listener.callback = [this](Event const* p_event) {
		TakeGameStateSnapshot();
	};


	EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);
}

void VkSceneRenderer::RenderScene(Image2D& output_image, Image2D& depth_image) {
	if (!mp_scene)
		return;

	auto* shadow_job = JobSystem::CreateWaitedOnJob();
	shadow_job->func = [this]([[maybe_unused]] Job const*) {m_shadow_pass.RecordCommandBuffers(*mp_scene, m_snapshot_data); };
	JobSystem::Execute(shadow_job);
	m_forward_pass.RecordCommandBuffer(output_image, depth_image, *mp_scene, m_snapshot_data);
	JobSystem::WaitOn(shadow_job);

	vk::SubmitInfo depth_submit_info;
	auto shadow_pass_cmd_buf = m_shadow_pass.GetCommandBuffer();
	depth_submit_info.setCommandBufferCount(1)
		.setPCommandBuffers(&shadow_pass_cmd_buf);

	SNK_CHECK_VK_RESULT(
		VkContext::GetLogicalDevice().graphics_queue.submit(depth_submit_info)
	);

	vk::SubmitInfo submit_info{};
	auto wait_stages = util::array<vk::PipelineStageFlags>(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	auto forward_cmd_buf = m_forward_pass.GetCommandBuffer();

	submit_info.pWaitDstStageMask = wait_stages.data();
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &forward_cmd_buf;
	submit_info.signalSemaphoreCount = 0;

	SNK_CHECK_VK_RESULT(
		VkContext::GetLogicalDevice().graphics_queue.submit(submit_info) // Fence is signalled once command buffer finishes execution
	);

}