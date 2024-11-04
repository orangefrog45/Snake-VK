#include "pch/pch.h"
#include "rendering/VkRenderer.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"
#include "resources/Images.h"

using namespace SNAKE;

void VkRenderer::InitImpl() {
	vk::SemaphoreCreateInfo semaphore_info{};

	vk::FenceCreateInfo fence_info{};
	fence_info.flags = vk::FenceCreateFlagBits::eSignaled; // Create in initially signalled state so execution begins immediately
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_image_avail_semaphores.push_back(std::move(VkContext::GetLogicalDevice().device->createSemaphoreUnique(semaphore_info).value));
		imgui_cmd_buffers[i].Init(vk::CommandBufferLevel::ePrimary);
		m_render_finished_semaphores[i] = std::move(VkContext::GetLogicalDevice().device->createSemaphoreUnique(semaphore_info).value);
		m_in_flight_fences[i] = std::move(VkContext::GetLogicalDevice().device->createFenceUnique(fence_info).value);

	}
	m_fence_sync_listener.callback = [this]([[maybe_unused]] auto _event) {
		SNK_CHECK_VK_RESULT(VkContext::GetLogicalDevice().device->waitForFences(1, &*m_in_flight_fences[VkContext::GetCurrentFIF()], VK_TRUE, UINT64_MAX));
		SNK_CHECK_VK_RESULT(VkContext::GetLogicalDevice().device->resetFences(1, &*m_in_flight_fences[VkContext::GetCurrentFIF()]));
		};

	EventManagerG::RegisterListener<FrameSyncFenceEvent>(m_fence_sync_listener);

	SNK_ASSERT(m_image_avail_semaphores[0]);
	vk::VertexInputAttributeDescription vert_input_desc{};
	vert_input_desc.binding = 0;
	vert_input_desc.format = vk::Format::eR32G32B32Sfloat;
	vert_input_desc.location = 0;
	vert_input_desc.offset = 0;
	vk::VertexInputBindingDescription binding_desc{};
	binding_desc.binding = 0;
	binding_desc.inputRate = vk::VertexInputRate::eVertex;
	binding_desc.stride = sizeof(glm::vec3);

	GraphicsPipelineBuilder debug_pipeline_builder{};
	debug_pipeline_builder.AddColourAttachment(vk::Format::eR8G8B8A8Srgb)
		.AddDepthAttachment(FindDepthFormat())
		.AddShader(vk::ShaderStageFlagBits::eVertex, "res/shaders/Transformvert_10000000.spv")
		.AddShader(vk::ShaderStageFlagBits::eFragment, "res/shaders/Colourfrag_00000000.spv");
	debug_pipeline_builder.input_assembly_info.topology = vk::PrimitiveTopology::eLineList;
	debug_pipeline_builder.Build();
	m_debug_pipeline.Init(debug_pipeline_builder);

}


void VkRenderer::RenderImGuiAndPresentImpl(Window& window, Image2D& render_image, vk::ImageView render_image_view) {
	auto cmd_buf = *imgui_cmd_buffers[VkContext::GetCurrentFIF()].buf;
	cmd_buf.reset();
	vk::CommandBufferBeginInfo begin_info{};
	SNK_CHECK_VK_RESULT(cmd_buf.begin(begin_info));

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
	SNK_CHECK_VK_RESULT(cmd_buf.end());

	vk::SubmitInfo submit_info{};
	submit_info.pSignalSemaphores = &*m_render_finished_semaphores[VkContext::GetCurrentFIF()];
	submit_info.signalSemaphoreCount = 1;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_buf;

	VkContext::GetLogicalDevice().SubmitGraphics(submit_info, *m_in_flight_fences[VkContext::GetCurrentFIF()]);
	PresentImage(window, *m_render_finished_semaphores[VkContext::GetCurrentFIF()]);
}

void VkRenderer::PresentImage(Window& window, vk::Semaphore wait_semaphore) {
	auto swapchains = util::array(*window.GetVkContext().swapchain);

	vk::PresentInfoKHR present_info{};
	present_info.swapchainCount = 1;
	present_info.pWaitSemaphores = &wait_semaphore;
	present_info.waitSemaphoreCount = 1;
	present_info.pSwapchains = swapchains.data();
	present_info.pImageIndices = &Get().m_current_swapchain_image_index;

	auto present_result = VkContext::GetLogicalDevice().SubmitPresentation(present_info);
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

	SNK_CHECK_VK_RESULT(VkContext::GetLogicalDevice().device->acquireNextImageKHR(*window.GetVkContext().swapchain, UINT64_MAX,
		*m_image_avail_semaphores[VkContext::GetCurrentFIF()], VK_NULL_HANDLE, &m_current_swapchain_image_index));

	image_index = m_current_swapchain_image_index;
	return *m_image_avail_semaphores[VkContext::GetCurrentFIF()];
}

void VkRenderer::RecordRenderDebugCommandsImpl(vk::CommandBuffer cmd_buf, Image2D& colour_image, Image2D& depth_image, DescriptorBuffer& scene_data_db) {
	vk::CommandBufferBeginInfo begin_info{};
	SNK_CHECK_VK_RESULT(cmd_buf.begin(begin_info));

	vk::RenderingAttachmentInfo colour_info{};
	colour_info.loadOp = vk::AttachmentLoadOp::eLoad;
	colour_info.storeOp = vk::AttachmentStoreOp::eStore;
	colour_info.imageView = colour_image.GetImageView();
	colour_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::RenderingAttachmentInfo depth_info{};
	depth_info.loadOp = vk::AttachmentLoadOp::eLoad;
	depth_info.storeOp = vk::AttachmentStoreOp::eStore;
	depth_info.imageView = depth_image.GetImageView();
	depth_info.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;

	const auto& colour_spec = colour_image.GetSpec();

	vk::RenderingInfo rendering_info{};
	rendering_info.colorAttachmentCount = 1;
	rendering_info.pColorAttachments = &colour_info;
	rendering_info.pDepthAttachment = &depth_info;
	rendering_info.layerCount = 1;
	rendering_info.renderArea.extent = vk::Extent2D{ colour_spec.size.x, colour_spec.size.y };
	rendering_info.renderArea.offset = vk::Offset2D{ 0, 0 };

	cmd_buf.beginRenderingKHR(rendering_info);
	cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, m_debug_pipeline.GetPipeline());

	vk::Viewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(colour_spec.size.x);
	viewport.height = static_cast<float>(colour_spec.size.y);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	cmd_buf.setViewport(0, 1, &viewport);
	vk::Rect2D scissor{};
	scissor.offset = vk::Offset2D{ 0, 0 };
	scissor.extent = rendering_info.renderArea.extent;
	cmd_buf.setScissor(0, 1, &scissor);

	vk::DescriptorBufferBindingInfoEXT binding_info = scene_data_db.GetBindingInfo();
	cmd_buf.bindDescriptorBuffersEXT(binding_info);

	uint32_t buffer_index_and_offset = 0;
	cmd_buf.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, m_debug_pipeline.pipeline_layout.GetPipelineLayout(), 0, buffer_index_and_offset, buffer_index_and_offset);

	for (auto& line : m_render_data.lines) {
		std::array<glm::vec4, 3> data = { glm::vec4(line.p0, 1.0), glm::vec4(line.p1, 1.0), glm::vec4(line.colour) };
		cmd_buf.pushConstants(m_debug_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAllGraphics, 0u, (uint32_t)(data.size() * sizeof(glm::vec4)), data.data());
		cmd_buf.draw(2, 1, 0, 0);
	}

	cmd_buf.endRenderingKHR();

	SNK_CHECK_VK_RESULT(cmd_buf.end());
}
