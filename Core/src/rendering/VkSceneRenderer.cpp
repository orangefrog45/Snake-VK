#include "components/MeshComponent.h"
#include "core/JobSystem.h"
#include "rendering/VkSceneRenderer.h"
#include "scene/Scene.h"
#include "scene/TransformBufferSystem.h"
#include "scene/SceneSnapshotSystem.h"

using namespace SNAKE;


void VkSceneRenderer::Init(Scene* p_scene) {
	mp_scene = p_scene;

	m_shadow_pass.Init();
	m_forward_pass.Init();
}

void VkSceneRenderer::RenderScene(Image2D& output_image, Image2D& depth_image, const SceneSnapshotData& snapshot_data) {
	if (!mp_scene)
		return;

	auto* shadow_job = JobSystem::CreateWaitedOnJob();
	shadow_job->func = [this, &snapshot_data]([[maybe_unused]] Job const*) {m_shadow_pass.RecordCommandBuffers(*mp_scene, snapshot_data); };
	JobSystem::Execute(shadow_job);
	m_forward_pass.RecordCommandBuffer(output_image, depth_image, *mp_scene, snapshot_data);
	JobSystem::WaitOn(shadow_job);

	vk::SubmitInfo depth_submit_info;
	auto shadow_pass_cmd_buf = m_shadow_pass.GetCommandBuffer();
	depth_submit_info.setCommandBufferCount(1)
		.setPCommandBuffers(&shadow_pass_cmd_buf);

	VkContext::GetLogicalDevice().SubmitGraphics(depth_submit_info);

	vk::SubmitInfo submit_info{};
	auto wait_stages = util::array<vk::PipelineStageFlags>(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	auto forward_cmd_buf = m_forward_pass.GetCommandBuffer();

	submit_info.pWaitDstStageMask = wait_stages.data();
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &forward_cmd_buf;
	submit_info.signalSemaphoreCount = 0;

	VkContext::GetLogicalDevice().SubmitGraphics(submit_info);

}