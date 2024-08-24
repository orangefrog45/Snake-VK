#include "pch/pch.h"
#include "rendering/VkRenderer.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"
#include "images/Images.h"

using namespace SNAKE;

void VkRenderer::InitImpl() {
	vk::SemaphoreCreateInfo semaphore_info{};

	vk::FenceCreateInfo fence_info{};
	fence_info.flags = vk::FenceCreateFlagBits::eSignaled; // Create in initially signalled state so execution begins immediately
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_image_avail_semaphores.push_back(std::move(VulkanContext::GetLogicalDevice().device->createSemaphoreUnique(semaphore_info).value));
		imgui_cmd_buffers[i].Init(vk::CommandBufferLevel::ePrimary);
		m_render_finished_semaphores[i] = std::move(VulkanContext::GetLogicalDevice().device->createSemaphoreUnique(semaphore_info).value);
		m_in_flight_fences[i] = std::move(VulkanContext::GetLogicalDevice().device->createFenceUnique(fence_info).value);

	}
	m_fence_sync_listener.callback = [this]([[maybe_unused]] auto _event) {
		SNK_CHECK_VK_RESULT(VulkanContext::GetLogicalDevice().device->waitForFences(1, &*m_in_flight_fences[VulkanContext::GetCurrentFIF()], VK_TRUE, UINT64_MAX));
		SNK_CHECK_VK_RESULT(VulkanContext::GetLogicalDevice().device->resetFences(1, &*m_in_flight_fences[VulkanContext::GetCurrentFIF()]));
		};

	EventManagerG::RegisterListener<FrameSyncFenceEvent>(m_fence_sync_listener);

	SNK_ASSERT(m_image_avail_semaphores[0]);
}


void VkRenderer::RenderImGuiAndPresentImpl(Window& window, Image2D& render_image, vk::ImageView render_image_view) {
	auto cmd_buf = *imgui_cmd_buffers[VulkanContext::GetCurrentFIF()].buf;
	VulkanContext::GetLogicalDevice().device->resetCommandPool(VulkanContext::GetCommandPool());
	vk::CommandBufferBeginInfo begin_info{};
	cmd_buf.begin(begin_info);

	vk::RenderingAttachmentInfo colour_info{};
	colour_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
	colour_info.loadOp = vk::AttachmentLoadOp::eLoad;
	colour_info.storeOp = vk::AttachmentStoreOp::eStore;
	colour_info.imageView = render_image_view;

	vk::RenderingInfo rendering_info{};
	rendering_info.colorAttachmentCount = 1;
	rendering_info.pColorAttachments = &colour_info;
	rendering_info.layerCount = 1;
	rendering_info.renderArea.extent = vk::Extent2D(render_image.GetSpec().size.x, render_image.GetSpec().size.y);
	cmd_buf.beginRendering(rendering_info);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buf);
	cmd_buf.endRendering();

	render_image.TransitionImageLayout(vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
		vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, 0, 1, cmd_buf);
	cmd_buf.end();

	vk::SubmitInfo submit_info{};
	submit_info.pSignalSemaphores = &*m_render_finished_semaphores[VulkanContext::GetCurrentFIF()];
	submit_info.signalSemaphoreCount = 1;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_buf;

	SNK_CHECK_VK_RESULT(
		VulkanContext::GetLogicalDevice().graphics_queue.submit(submit_info, *m_in_flight_fences[VulkanContext::GetCurrentFIF()])
	);

	PresentImage(window, *m_render_finished_semaphores[VulkanContext::GetCurrentFIF()]);
}

void VkRenderer::PresentImage(Window& window, vk::Semaphore wait_semaphore) {
	auto swapchains = util::array(*window.GetVkContext().swapchain);

	vk::PresentInfoKHR present_info{};
	present_info.swapchainCount = 1;
	present_info.pWaitSemaphores = &wait_semaphore;
	present_info.waitSemaphoreCount = 1;
	present_info.pSwapchains = swapchains.data();
	present_info.pImageIndices = &Get().m_current_swapchain_image_index;

	auto present_result = VulkanContext::GetLogicalDevice().presentation_queue.presentKHR(present_info);
	if (present_result == vk::Result::eErrorOutOfDateKHR || window.WasJustResized() || present_result == vk::Result::eSuboptimalKHR) {
		window.OnPresentNeedsResize();
		EventManagerG::DispatchEvent(SwapchainInvalidateEvent{ {window.GetWidth(), window.GetHeight()} });
	}
	else if (present_result != vk::Result::eSuccess) {
		SNK_BREAK("Failed to present image");
	}

	Get().m_current_swapchain_image_index = READY_TO_REACQUIRE_SWAPCHAIN_IDX;
}

vk::Semaphore VkRenderer::AcquireNextSwapchainImageImpl(Window& window, uint32_t& image_index) {
	SNK_ASSERT_ARG(m_current_swapchain_image_index == READY_TO_REACQUIRE_SWAPCHAIN_IDX,
		"Not ready to reacquire image, ensure VkRenderer::PresentImage or VkRenderer::RenderImGuiAndPresent is being called at the end of each frame");

	SNK_CHECK_VK_RESULT(VulkanContext::GetLogicalDevice().device->acquireNextImageKHR(*window.GetVkContext().swapchain, UINT64_MAX,
		*m_image_avail_semaphores[VulkanContext::GetCurrentFIF()], VK_NULL_HANDLE, &m_current_swapchain_image_index));

	image_index = m_current_swapchain_image_index;
	return *m_image_avail_semaphores[VulkanContext::GetCurrentFIF()];
}