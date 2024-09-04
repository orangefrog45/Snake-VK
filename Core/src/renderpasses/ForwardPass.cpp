#include "pch/pch.h"
#include "assets/AssetManager.h"
#include "components/MeshComponent.h"
#include "renderpasses/ForwardPass.h"
#include "scene/LightBufferSystem.h"
#include "scene/Scene.h"
#include "scene/SceneInfoBufferSystem.h"
#include "scene/TransformBufferSystem.h"
#include "assets/MaterialAsset.h"

namespace SNAKE {

	void ForwardPass::Init(Window* p_window, Image2D* p_shadowmap_image) {
		// Create command buffers
		for (auto& buf : m_cmd_buffers) {
			buf.Init(vk::CommandBufferLevel::ePrimary);
		}

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			auto main_pass_descriptor_set_spec = std::make_shared<DescriptorSetSpec>();
			main_pass_descriptor_set_spec->AddDescriptor(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAllGraphics)
				.GenDescriptorLayout();

			auto shadow_map_info = p_shadowmap_image->CreateDescriptorGetInfo(vk::ImageLayout::eShaderReadOnlyOptimal);
		}

		// Create graphics pipeline
		auto binding_desc = Vertex::GetBindingDescription();
		auto attribute_desc = Vertex::GetAttributeDescriptions();

		GraphicsPipelineBuilder graphics_builder{};
		graphics_builder.AddShader(vk::ShaderStageFlagBits::eVertex, "res/shaders/Forwardvert_00000000.spv")
			.AddShader(vk::ShaderStageFlagBits::eFragment, "res/shaders/Forwardfrag_00000000.spv")
			.AddVertexBinding(attribute_desc[0], binding_desc[0])
			.AddVertexBinding(attribute_desc[1], binding_desc[1])
			.AddVertexBinding(attribute_desc[2], binding_desc[2])
			.AddVertexBinding(attribute_desc[3], binding_desc[3])
			.AddColourAttachment(vk::Format::eR8G8B8A8Srgb)
			.AddDepthAttachment(FindDepthFormat())
			.Build();

		m_graphics_pipeline.Init(graphics_builder);
	}



	void ForwardPass::RecordCommandBuffer(Image2D& output_image, Image2D& depth_image, Scene& scene, const SceneSnapshotData& snapshot) {
		auto frame_idx = VkContext::GetCurrentFIF();

		auto cmd_buffer = *m_cmd_buffers[frame_idx].buf;
		SNK_CHECK_VK_RESULT(cmd_buffer.reset());
		vk::CommandBufferBeginInfo begin_info{};
		SNK_CHECK_VK_RESULT(
			m_cmd_buffers[frame_idx].buf->begin(&begin_info)
		);

		output_image.TransitionImageLayout( vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal, 0, 1, *m_cmd_buffers[frame_idx].buf);

		vk::RenderingAttachmentInfo colour_attachment_info{};
		colour_attachment_info.loadOp = vk::AttachmentLoadOp::eClear; // Clear initially
		colour_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
		colour_attachment_info.imageView = output_image.GetImageView();
		colour_attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;

		vk::RenderingAttachmentInfo depth_attachment_info{};
		depth_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
		depth_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
		depth_attachment_info.imageView = depth_image.GetImageView();
		depth_attachment_info.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
		depth_attachment_info.clearValue = vk::ClearValue{ vk::ClearDepthStencilValue{1.f} };

		vk::RenderingInfo render_info{};
		render_info.layerCount = 1;
		render_info.colorAttachmentCount = 1;
		render_info.pColorAttachments = &colour_attachment_info;
		render_info.pDepthAttachment = &depth_attachment_info;
		auto& spec = output_image.GetSpec();
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

		vk::DescriptorBufferBindingInfoEXT transform_binding_info{};
		transform_binding_info.address = scene.GetSystem<TransformBufferSystem>()->GetTransformStorageBuffer(VkContext::GetCurrentFIF()).GetDeviceAddress();
		transform_binding_info.usage = vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT;

		std::array<vk::DescriptorBufferBindingInfoEXT, 4> binding_infos = { scene.GetSystem<SceneInfoBufferSystem>()->descriptor_buffers[frame_idx].GetBindingInfo(),
			AssetManager::GetGlobalTexMatBufBindingInfo(frame_idx), scene.GetSystem<LightBufferSystem>()->light_descriptor_buffers[frame_idx].GetBindingInfo(),
			 transform_binding_info };

		cmd_buffer.bindDescriptorBuffersEXT(binding_infos);

		std::array<uint32_t, 4> buffer_indices = { 0, 1, 2, 3 };
		std::array<vk::DeviceSize, 4> buffer_offsets = { 0, 0, 0, 0 };

		cmd_buffer.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline.pipeline_layout.GetPipelineLayout(), 0, buffer_indices, buffer_offsets);
		
		auto& asset_manager = AssetManager::Get();
		auto buffers = asset_manager.mesh_buffer_manager.GetMeshBuffers();

		std::vector<vk::Buffer> vert_buffers = { buffers.position_buf.buffer, buffers.normal_buf.buffer, buffers.tex_coord_buf.buffer, buffers.tangent_buf.buffer };
		std::vector<vk::Buffer> index_buffers = { buffers.indices_buf.buffer };

		for (auto& range : snapshot.mesh_ranges) {
			auto mesh_asset = AssetManager::GetAssetRaw<StaticMeshAsset>(range.mesh_uuid);
			auto& mesh_buffer_entry_data = asset_manager.mesh_buffer_manager.GetEntryData(mesh_asset->data.get());

			std::vector<vk::DeviceSize> offsets = { mesh_buffer_entry_data.data_start_vertex_idx, mesh_buffer_entry_data.data_start_vertex_idx, 
				mesh_buffer_entry_data.data_start_vertex_idx, mesh_buffer_entry_data.data_start_vertex_idx };

			cmd_buffer.bindVertexBuffers(0, 4, vert_buffers.data(), offsets.data());
			cmd_buffer.bindIndexBuffer(index_buffers[0], mesh_buffer_entry_data.data_start_indices_idx, vk::IndexType::eUint32);

			for (uint32_t i = range.start_idx; i < range.start_idx + range.count; i++) {
				cmd_buffer.pushConstants(m_graphics_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAllGraphics, 0, sizeof(uint32_t), &i);
				for (auto& submesh : mesh_asset->data->submeshes) {
					uint32_t mat_index = (*snapshot.static_mesh_data[i].material_vec)[submesh.material_index]->GetGlobalBufferIndex();
					cmd_buffer.pushConstants(m_graphics_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAllGraphics, sizeof(uint32_t), sizeof(uint32_t), &mat_index);
					cmd_buffer.drawIndexed(submesh.num_indices, 1, submesh.base_index, submesh.base_vertex, 0);
				}
			}
		}
		
		cmd_buffer.endRenderingKHR();

		SNK_CHECK_VK_RESULT(
			cmd_buffer.end()
		);
	}
}