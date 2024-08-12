#pragma once
#include "renderpasses/ForwardPass.h"
#include "renderpasses/ShadowPass.h"

namespace SNAKE {
	class VkSceneRenderer {
	public:
		void Init(Window& window) {
			m_shadow_pass.Init();
			m_forward_pass.Init(&window, &m_shadow_pass.m_dir_light_shadow_map);

			vk::SemaphoreCreateInfo semaphore_info{};
			vk::FenceCreateInfo fence_info{};
			fence_info.flags = vk::FenceCreateFlagBits::eSignaled; // Create in initially signalled state so execution begins immediately

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				m_in_flight_fences[i] = std::move(VulkanContext::GetLogicalDevice().device->createFenceUnique(fence_info).value);
				m_render_finished_semaphores[i] = std::move(VulkanContext::GetLogicalDevice().device->createSemaphoreUnique(semaphore_info).value);
			}

			m_fence_sync_listener.callback = [this]([[maybe_unused]] auto _event) {
				SNK_CHECK_VK_RESULT(VulkanContext::GetLogicalDevice().device->waitForFences(1, &*m_in_flight_fences[VulkanContext::GetCurrentFIF()], VK_TRUE, UINT64_MAX));
				};

			EventManagerG::RegisterListener<FrameSyncFenceEvent>(m_fence_sync_listener);
		}

		// Returns semaphore to wait on before presenting image
		vk::Semaphore RenderScene(class Scene* p_scene, Image2D& output_image, vk::Semaphore forward_wait_semaphore) {
			auto frame_idx = VulkanContext::GetCurrentFIF();

			SNK_CHECK_VK_RESULT(
				VulkanContext::GetLogicalDevice().device->resetFences(1, &*m_in_flight_fences[frame_idx])
			);

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
			auto signal_semaphores = util::array(*m_render_finished_semaphores[frame_idx]); 
			auto forward_cmd_buf = m_forward_pass.GetCommandBuffer();

			submit_info.waitSemaphoreCount = 1;
			submit_info.pWaitSemaphores = &forward_wait_semaphore;
			submit_info.pWaitDstStageMask = wait_stages.data();
			submit_info.commandBufferCount = 1;
			submit_info.pCommandBuffers = &forward_cmd_buf;
			submit_info.signalSemaphoreCount = 1;
			submit_info.pSignalSemaphores = signal_semaphores.data();

			SNK_CHECK_VK_RESULT(
				VulkanContext::GetLogicalDevice().graphics_queue.submit(1, &submit_info, *m_in_flight_fences[frame_idx]) // Fence is signalled once command buffer finishes execution
			);

			return *m_render_finished_semaphores[frame_idx];
		}

	private:
		ForwardPass m_forward_pass;
		ShadowPass m_shadow_pass;

		EventListener m_fence_sync_listener;

		std::array<vk::UniqueFence, MAX_FRAMES_IN_FLIGHT> m_in_flight_fences;
		std::array<vk::UniqueSemaphore, MAX_FRAMES_IN_FLIGHT> m_render_finished_semaphores;

	};
}