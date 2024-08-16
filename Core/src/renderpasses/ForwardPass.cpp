#include "pch/pch.h"
#include "renderpasses/ForwardPass.h"
#include "scene/Scene.h"
#include "scene/SceneInfoBufferSystem.h"
#include "scene/LightBufferSystem.h"
#include "assets/AssetManager.h"
#include "components/MeshComponent.h"

namespace SNAKE {

	void ForwardPass::Init(Window* p_window, Image2D* p_shadowmap_image) {
		// Create command buffers
		for (auto& buf : m_cmd_buffers) {
			buf.Init();
		}

		CreateDepthResources({p_window->GetWidth(), p_window->GetHeight()});

		// Create descriptor buffers
		auto& device = VulkanContext::GetLogicalDevice().device;
		auto& descriptor_buffer_properties = VulkanContext::GetPhysicalDevice().buffer_properties;

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			auto main_pass_descriptor_set_spec = std::make_shared<DescriptorSetSpec>();
			main_pass_descriptor_set_spec->AddDescriptor(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAllGraphics)
				.GenDescriptorLayout();

			m_main_pass_descriptor_buffers[i].SetDescriptorSpec(main_pass_descriptor_set_spec);
			m_main_pass_descriptor_buffers[i].CreateBuffer(1);

			auto shadow_map_info = p_shadowmap_image->CreateDescriptorGetInfo(vk::ImageLayout::eShaderReadOnlyOptimal);
			m_main_pass_descriptor_buffers[i].LinkResource(shadow_map_info.first, 0, 0);

		}

		// Create graphics pipeline
		auto binding_desc = Vertex::GetBindingDescription();
		auto attribute_desc = Vertex::GetAttributeDescriptions();

		GraphicsPipelineBuilder graphics_builder{};
		graphics_builder.AddShader(vk::ShaderStageFlagBits::eVertex, "res/shaders/vert.spv")
			.AddShader(vk::ShaderStageFlagBits::eFragment, "res/shaders/frag.spv")
			.AddVertexBinding(attribute_desc[0], binding_desc[0])
			.AddVertexBinding(attribute_desc[1], binding_desc[1])
			.AddVertexBinding(attribute_desc[2], binding_desc[2])
			.AddColourAttachment(p_window->GetVkContext().swapchain_format)
			.AddDepthAttachment(FindDepthFormat())
			.Build();

		m_graphics_pipeline.Init(graphics_builder);
	}


	void ForwardPass::CreateDepthResources(glm::vec2 window_dimensions) {
		// Create depth image
		auto depth_format = FindDepthFormat();
		Image2DSpec depth_spec{};
		depth_spec.format = depth_format;
		depth_spec.size = { window_dimensions.x, window_dimensions.y };
		depth_spec.tiling = vk::ImageTiling::eOptimal;
		depth_spec.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
		depth_spec.aspect_flags = vk::ImageAspectFlagBits::eDepth | (HasStencilComponent(depth_format) ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eNone);
		m_depth_image.SetSpec(depth_spec);
		m_depth_image.CreateImage();
		m_depth_image.CreateImageView() ;

		m_depth_image.TransitionImageLayout(vk::ImageLayout::eUndefined,
			(HasStencilComponent(depth_format) ? vk::ImageLayout::eDepthAttachmentOptimal : vk::ImageLayout::eDepthAttachmentOptimal));
	}

	void ForwardPass::RecordCommandBuffer(Image2D& output_image, Scene& scene) {
		auto frame_idx = VulkanContext::GetCurrentFIF();

		m_cmd_buffers[frame_idx].buf->reset();

		auto cmd_buffer = *m_cmd_buffers[frame_idx].buf;

		vk::CommandBufferBeginInfo begin_info{};
		SNK_CHECK_VK_RESULT(
			m_cmd_buffers[frame_idx].buf->begin(&begin_info)
		);

		output_image.TransitionImageLayout( vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal, *m_cmd_buffers[frame_idx].buf);

		vk::RenderingAttachmentInfo colour_attachment_info{};
		colour_attachment_info.loadOp = vk::AttachmentLoadOp::eClear; // Clear initially
		colour_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
		colour_attachment_info.imageView = output_image.GetImageView();
		colour_attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;

		vk::RenderingAttachmentInfo depth_attachment_info{};
		depth_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
		depth_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
		depth_attachment_info.imageView = m_depth_image.GetImageView();
		depth_attachment_info.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
		depth_attachment_info.clearValue = vk::ClearValue{ vk::ClearDepthStencilValue{1.f} };

		vk::RenderingInfo render_info{};
		render_info.layerCount = 1;
		render_info.colorAttachmentCount = 1;
		render_info.pColorAttachments = &colour_attachment_info;
		render_info.pDepthAttachment = &depth_attachment_info;
		auto spec = output_image.GetSpec();
		render_info.renderArea.extent = vk::Extent2D{ spec.size.x, spec.size.y };
		render_info.renderArea.offset = vk::Offset2D{ 0, 0 };

		cmd_buffer.beginRenderingKHR(
			render_info
		);

		// Binds shaders and sets all state like rasterizer, blend, multisampling etc
		cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline.GetPipeline());

		vk::Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(spec.size.x);
		viewport.height = static_cast<float>(spec.size.y);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd_buffer.setViewport(0, 1, &viewport);
		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = render_info.renderArea.extent;
		cmd_buffer.setScissor(0, 1, &scissor);


		std::array<vk::DescriptorBufferBindingInfoEXT, 4> binding_infos = { scene.GetSystem<SceneInfoBufferSystem>()->descriptor_buffers[frame_idx].GetBindingInfo(),
			AssetManager::GetGlobalTexMatBufBindingInfo(frame_idx), scene.GetSystem<LightBufferSystem>()->light_descriptor_buffers[frame_idx].GetBindingInfo(),
			 m_main_pass_descriptor_buffers[frame_idx].GetBindingInfo() };

		cmd_buffer.bindDescriptorBuffersEXT(binding_infos);

		std::array<uint32_t, 4> buffer_indices = { 0, 1, 2, 3 };
		std::array<vk::DeviceSize, 4> buffer_offsets = { 0, 0, 0, 0 };

		StaticMeshDataAsset* p_last_bound_data = nullptr;

		cmd_buffer.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline.pipeline_layout.GetPipelineLayout(), 0, buffer_indices, buffer_offsets);
		for (auto [entity, mesh, transform] : scene.GetRegistry().view<StaticMeshComponent, TransformComponent>().each()) {
			if (p_last_bound_data != mesh.mesh_asset->data.get()) {
				std::vector<vk::Buffer> vert_buffers = { mesh.mesh_asset->data->position_buf.buffer, mesh.mesh_asset->data->normal_buf.buffer, mesh.mesh_asset->data->tex_coord_buf.buffer };
				std::vector<vk::Buffer> index_buffers = { mesh.mesh_asset->data->index_buf.buffer };
				std::vector<vk::DeviceSize> offsets = { 0, 0, 0 };
				cmd_buffer.bindVertexBuffers(0, 3, vert_buffers.data(), offsets.data());
				cmd_buffer.bindIndexBuffer(mesh.mesh_asset->data->index_buf.buffer, 0, vk::IndexType::eUint32);
				p_last_bound_data = mesh.mesh_asset->data.get();
			}

			cmd_buffer.pushConstants(m_graphics_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAllGraphics, 0, sizeof(glm::mat4), &transform.GetMatrix()[0][0]);
			cmd_buffer.drawIndexed((uint32_t)mesh.mesh_asset->data->index_buf.alloc_info.size / sizeof(uint32_t), 1, 0, 0, 0);
		}

		cmd_buffer.endRenderingKHR();


		SNK_CHECK_VK_RESULT(
			cmd_buffer.end()
		);
	}
}