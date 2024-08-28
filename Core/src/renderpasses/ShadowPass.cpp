#include "renderpasses/ShadowPass.h"
#include "scene/Scene.h"
#include "scene/LightBufferSystem.h"
#include "components/MeshComponent.h"
#include "core/JobSystem.h"

namespace SNAKE {
	void ShadowPass::Init() {
		Image2DSpec shadow_spec;
		shadow_spec.size = { 4096, 4096 };
		shadow_spec.format = vk::Format::eD16Unorm;
		shadow_spec.tiling = vk::ImageTiling::eOptimal;
		shadow_spec.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
		shadow_spec.aspect_flags = vk::ImageAspectFlagBits::eDepth;

		m_dir_light_shadow_map.SetSpec(shadow_spec);
		m_dir_light_shadow_map.CreateImage();
		m_dir_light_shadow_map.CreateImageView();
		m_dir_light_shadow_map.CreateSampler();

		m_dir_light_shadow_map.TransitionImageLayout(vk::ImageLayout::eUndefined,
			vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eNone,
			vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, 0);

		PipelineLayoutBuilder layout_builder{};
		DescriptorSetSpec spec{};
		spec.GenDescriptorLayout();

		auto vert_binding_descs = Vertex::GetBindingDescription();
		auto vert_attr_descs = Vertex::GetAttributeDescriptions();

		GraphicsPipelineBuilder builder{};
		builder.AddDepthAttachment(vk::Format::eD16Unorm)
			.AddShader(vk::ShaderStageFlagBits::eVertex, "res/shaders/Depthvert_00000000.spv")
			.AddShader(vk::ShaderStageFlagBits::eFragment, "res/shaders/Depthfrag_00000000.spv")
			.AddVertexBinding(vert_attr_descs[0], vert_binding_descs[0])
			.AddVertexBinding(vert_attr_descs[1], vert_binding_descs[1])
			.AddVertexBinding(vert_attr_descs[2], vert_binding_descs[2])
			.Build();

		m_pipeline.Init(builder);

		auto alloc_cmd_bufs_job = JobSystem::CreateWaitedOnJob();
		alloc_cmd_bufs_job->func = [&](Job const*) {
			for (auto& buf : m_cmd_buffers) {
				buf.Init(vk::CommandBufferLevel::ePrimary);
			}
		};
		JobSystem::Execute(alloc_cmd_bufs_job);
		JobSystem::WaitOn(alloc_cmd_bufs_job);
	}

	void ShadowPass::RecordCommandBuffers(Scene& scene, const SceneSnapshotData& snapshot) {
		auto frame_idx = VulkanContext::GetCurrentFIF();

		auto cmd = *m_cmd_buffers[frame_idx].buf;
		SNK_CHECK_VK_RESULT(cmd.reset());
		vk::CommandBufferBeginInfo begin_info{};
		SNK_CHECK_VK_RESULT(cmd.begin(&begin_info));

		m_dir_light_shadow_map.TransitionImageLayout( vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::ImageLayout::eDepthAttachmentOptimal, vk::AccessFlagBits::eNone,
			vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eEarlyFragmentTests, 0, 1, cmd);


		vk::RenderingAttachmentInfo depth_info;
		depth_info.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
			.setImageView(m_dir_light_shadow_map.GetImageView())
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setClearValue(vk::ClearDepthStencilValue{ 1.f });


		vk::RenderingInfo render_info{};
		render_info.layerCount = 1;
		render_info.pDepthAttachment = &depth_info;
		render_info.renderArea.extent = vk::Extent2D{ 4096, 4096 };
		render_info.renderArea.offset = vk::Offset2D{ 0, 0 };

		cmd.beginRenderingKHR(
			render_info
		);

		// Binds shaders and sets all state like rasterizer, blend, multisampling etc
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.GetPipeline());

		vk::Viewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = 4096;
		viewport.height = 4096;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		cmd.setViewport(0, 1, &viewport);
		vk::Rect2D scissor{};
		scissor.offset = vk::Offset2D{ 0, 0 };
		scissor.extent = vk::Extent2D(4096, 4096);
		cmd.setScissor(0, 1, &scissor);

		vk::DescriptorBufferBindingInfoEXT light_buffer_binding_info{};
		light_buffer_binding_info.address = scene.GetSystem<LightBufferSystem>()->light_descriptor_buffers[frame_idx].descriptor_buffer.GetDeviceAddress();
		light_buffer_binding_info.usage = vk::BufferUsageFlagBits::eResourceDescriptorBufferEXT;

		cmd.bindDescriptorBuffersEXT(light_buffer_binding_info);
		std::array<uint32_t, 1> buffer_indices = { 0 };
		std::array<vk::DeviceSize, 1> buffer_offsets = { 0 };

		cmd.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, m_pipeline.pipeline_layout.GetPipelineLayout(), (uint32_t)DescriptorSetIndices::LIGHTS, buffer_indices, buffer_offsets);

		for (auto& range : snapshot.mesh_ranges) {
			auto mesh_asset = AssetManager::GetAsset<StaticMeshAsset>(range.mesh_uuid);
			std::vector<vk::Buffer> vert_buffers = { mesh_asset->data->position_buf.buffer, mesh_asset->data->normal_buf.buffer, mesh_asset->data->tex_coord_buf.buffer };
			std::vector<vk::Buffer> index_buffers = { mesh_asset->data->index_buf.buffer };
			std::vector<vk::DeviceSize> offsets = { 0, 0, 0 };
			cmd.bindVertexBuffers(0, 3, vert_buffers.data(), offsets.data());
			cmd.bindIndexBuffer(mesh_asset->data->index_buf.buffer, 0, vk::IndexType::eUint32);

			for (uint32_t i = range.start_idx; i < range.start_idx + range.count; i++) {
				cmd.pushConstants(m_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAllGraphics, 0, sizeof(glm::mat4), &snapshot.static_mesh_data[i].transform);
				for (auto& submesh : mesh_asset->data->submeshes) {
					cmd.drawIndexed(submesh.num_indices, 1, submesh.base_index, submesh.base_vertex, 0);
				}
			}
		}
	
		cmd.endRenderingKHR();

		m_dir_light_shadow_map.TransitionImageLayout(vk::ImageLayout::eDepthAttachmentOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eNone,
			vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eLateFragmentTests, vk::PipelineStageFlagBits::eLateFragmentTests | vk::PipelineStageFlagBits::eEarlyFragmentTests, 0, 1, cmd);

		SNK_CHECK_VK_RESULT(cmd.end());

	}
}