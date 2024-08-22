#pragma once
#include "renderpasses/ForwardPass.h"
#include "renderpasses/ShadowPass.h"

namespace SNAKE {
	class VkSceneRenderer {
	public:
		void Init(Window& window) {
			m_shadow_pass.Init();
			m_forward_pass.Init(&window, &m_shadow_pass.m_dir_light_shadow_map);

		}

		void RenderScene(class Scene* p_scene, Image2D& output_image, std::optional<vk::Semaphore> forward_wait_semaphore = std::nullopt) {
			m_shadow_pass.RecordCommandBuffers(p_scene);
			m_forward_pass.RecordCommandBuffer(output_image, *p_scene);

			vk::SubmitInfo depth_submit_info;
			auto shadow_pass_cmd_buf = m_shadow_pass.GetCommandBuffer();
			depth_submit_info.setCommandBufferCount(1)
				.setPCommandBuffers(&shadow_pass_cmd_buf);

			SNK_CHECK_VK_RESULT(
				VulkanContext::GetLogicalDevice().graphics_queue.submit(depth_submit_info)
			);

			vk::SubmitInfo submit_info{};
			auto wait_stages = util::array<vk::PipelineStageFlags>(vk::PipelineStageFlagBits::eColorAttachmentOutput);
			auto forward_cmd_buf = m_forward_pass.GetCommandBuffer();

			if (forward_wait_semaphore.has_value()) {
				submit_info.waitSemaphoreCount = 1;
				submit_info.pWaitSemaphores = &forward_wait_semaphore.value();
			}

			submit_info.pWaitDstStageMask = wait_stages.data();
			submit_info.commandBufferCount = 1;
			submit_info.pCommandBuffers = &forward_cmd_buf;
			submit_info.signalSemaphoreCount = 0;

			SNK_CHECK_VK_RESULT(
				VulkanContext::GetLogicalDevice().graphics_queue.submit(submit_info) // Fence is signalled once command buffer finishes execution
			);

		}

	private:
		ForwardPass m_forward_pass;
		ShadowPass m_shadow_pass;
	};
}