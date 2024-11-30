#include "pch/pch.h"
#include "renderpasses/GBufferPass.h"
#include "scene/SceneInfoBufferSystem.h"
#include "scene/TransformBufferSystem.h"
#include "scene/SceneSnapshotSystem.h"
#include "scene/ParticleSystem.h"

using namespace SNAKE;

void GBufferPass::Init(Scene& scene, GBufferResources& output) {
	auto binding_desc = Vertex::GetBindingDescription();
	auto attribute_desc = Vertex::GetAttributeDescriptions();

	GraphicsPipelineBuilder gp_builder_mesh{};
	GraphicsPipelineBuilder gp_builder_ptcl{};

	gp_builder_mesh.AddColourAttachment(output.albedo_image.GetSpec().format)
		.AddColourAttachment(output.normal_image.GetSpec().format)
		.AddColourAttachment(output.rma_image.GetSpec().format)
		.AddColourAttachment(output.pixel_motion_image.GetSpec().format)
		.AddColourAttachment(output.mat_flag_image.GetSpec().format)
		.AddDepthAttachment(output.depth_image.GetSpec().format)
		.AddShader(vk::ShaderStageFlagBits::eVertex, "res/shaders/GBuffervert_10000000.spv")
		.AddShader(vk::ShaderStageFlagBits::eFragment, "res/shaders/GBufferfrag_10000000.spv")
		.AddVertexBinding(attribute_desc[0], binding_desc[0])
		.AddVertexBinding(attribute_desc[1], binding_desc[1])
		.AddVertexBinding(attribute_desc[2], binding_desc[2])
		.AddVertexBinding(attribute_desc[3], binding_desc[3])
		.Build();

	gp_builder_ptcl.AddColourAttachment(output.albedo_image.GetSpec().format)
		.AddColourAttachment(output.normal_image.GetSpec().format)
		.AddColourAttachment(output.rma_image.GetSpec().format)
		.AddColourAttachment(output.pixel_motion_image.GetSpec().format)
		.AddColourAttachment(output.mat_flag_image.GetSpec().format)
		.AddDepthAttachment(output.depth_image.GetSpec().format)
		.AddShader(vk::ShaderStageFlagBits::eVertex, "res/shaders/GBuffervert_01000000.spv")
		.AddShader(vk::ShaderStageFlagBits::eFragment, "res/shaders/GBufferfrag_01000000.spv")
		.AddVertexBinding(attribute_desc[0], binding_desc[0])
		.AddVertexBinding(attribute_desc[1], binding_desc[1])
		.AddVertexBinding(attribute_desc[2], binding_desc[2])
		.AddVertexBinding(attribute_desc[3], binding_desc[3])
		.Build();

	m_mesh_pipeline.Init(gp_builder_mesh);
	m_particle_pipeline.Init(gp_builder_ptcl);

	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_descriptor_buffers[i].SetDescriptorSpec(m_mesh_pipeline.pipeline_layout.GetDescriptorSetLayout(2));
		m_descriptor_buffers[i].CreateBuffer(1);
		auto* p_transform_system = scene.GetSystem<TransformBufferSystem>();
		auto* p_ptcl_system = scene.GetSystem<ParticleSystem>();
		auto transform_get_info = p_transform_system->GetTransformStorageBuffer(i).CreateDescriptorGetInfo();
		auto prev_frame_transform_get_info = p_transform_system->GetLastFramesTransformStorageBuffer(i).CreateDescriptorGetInfo();

		auto& ptcl_buf = p_ptcl_system->GetParticleBuffer(i);
		auto ptcl_buf_get_info = ptcl_buf.CreateDescriptorGetInfo();

		auto& ptcl_buf_prev_frame = p_ptcl_system->GetPrevFrameParticleBuffer(i);
		auto ptcl_buf_prev_frame_get_info = ptcl_buf_prev_frame.CreateDescriptorGetInfo();

		m_descriptor_buffers[i].LinkResource(&p_transform_system->GetTransformStorageBuffer(i), transform_get_info, 0, 0);
		m_descriptor_buffers[i].LinkResource(&p_transform_system->GetLastFramesTransformStorageBuffer(i), prev_frame_transform_get_info, 1, 0);
		m_descriptor_buffers[i].LinkResource(&ptcl_buf, ptcl_buf_get_info, 2, 0);
		m_descriptor_buffers[i].LinkResource(&ptcl_buf_prev_frame, ptcl_buf_prev_frame_get_info, 3, 0);
	}
}

// output_size - The size of the final colour output image produced in the pipeline (if DLSS is enabled this is different from the size of the gbuffer resources)
void GBufferPass::RecordCommandBuffer(GBufferResources& output, Scene& scene, glm::uvec2 output_size, vk::CommandBuffer cmd_buffer) {
	auto frame_idx = VkContext::GetCurrentFIF();

	output.normal_image.BlitTo(output.prev_frame_normal_image, 0, 0, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::Filter::eNearest, std::nullopt, cmd_buffer);

	output.depth_image.BlitTo(output.prev_frame_depth_image, 0, 0, vk::ImageLayout::eDepthAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::ImageLayout::eDepthAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::Filter::eNearest, std::nullopt, cmd_buffer);

	auto depth_attachment_info = output.depth_image.CreateAttachmentInfo(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ImageLayout::eDepthAttachmentOptimal, { 1.f, 1.f, 1.f, 1.f });
	auto colour_attachments = util::array(
		output.albedo_image.CreateAttachmentInfo(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ImageLayout::eColorAttachmentOptimal), 
		output.normal_image.CreateAttachmentInfo(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ImageLayout::eColorAttachmentOptimal),
		output.rma_image.CreateAttachmentInfo(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ImageLayout::eColorAttachmentOptimal), 
		output.pixel_motion_image.CreateAttachmentInfo(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ImageLayout::eColorAttachmentOptimal),
		output.mat_flag_image.CreateAttachmentInfo(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ImageLayout::eColorAttachmentOptimal)
		);

	vk::RenderingInfo render_info{};
	render_info.layerCount = 1;
	render_info.colorAttachmentCount = (uint32_t)colour_attachments.size();
	render_info.pColorAttachments = colour_attachments.data();
	render_info.pDepthAttachment = &depth_attachment_info;
	auto& spec = output.depth_image.GetSpec();
	render_info.renderArea.extent = vk::Extent2D{ spec.size.x, spec.size.y };
	render_info.renderArea.offset = vk::Offset2D{ 0, 0 };

	cmd_buffer.beginRenderingKHR(
		render_info
	);

	// Binds shaders and sets all state like rasterizer, blend, multisampling etc
	cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_mesh_pipeline.GetPipeline());

	vk::Viewport viewport = CreateDefaultVkViewport((float)spec.size.x, (float)spec.size.y);
	cmd_buffer.setViewport(0, 1, &viewport);

	vk::Rect2D scissor{};
	scissor.offset = vk::Offset2D{ 0, 0 };
	scissor.extent = render_info.renderArea.extent;
	cmd_buffer.setScissor(0, 1, &scissor);

	auto* p_scene_info_buffer_sys = scene.GetSystem<SceneInfoBufferSystem>();

	std::array<vk::DescriptorBufferBindingInfoEXT, 3> binding_infos = {
		p_scene_info_buffer_sys->descriptor_buffers[frame_idx].GetBindingInfo(),
		AssetManager::GetGlobalTexMatBufBindingInfo(frame_idx),
		m_descriptor_buffers[VkContext::GetCurrentFIF()].GetBindingInfo()
	};

	cmd_buffer.bindDescriptorBuffersEXT(binding_infos);

	std::array<uint32_t, 3> buffer_indices = { 0, 1, 2 };
	std::array<vk::DeviceSize, 3> buffer_offsets = { 0, 0, 0 };

	cmd_buffer.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, m_mesh_pipeline.pipeline_layout.GetPipelineLayout(), 0, buffer_indices, buffer_offsets);

	auto& asset_manager = AssetManager::Get();
	auto buffers = asset_manager.mesh_buffer_manager.GetMeshBuffers();

	std::vector<vk::Buffer> vert_buffers = { buffers.position_buf.buffer, buffers.normal_buf.buffer, buffers.tex_coord_buf.buffer, buffers.tangent_buf.buffer };
	std::vector<vk::Buffer> index_buffers = { buffers.indices_buf.buffer };

	GBufferPC pc;
	{
		pc.render_resolution = output.albedo_image.GetSpec().size;
		pc.jitter_offset = GetFrameJitter(VkContext::GetCurrentFrameIdx(), pc.render_resolution.x, output_size.x);
		cmd_buffer.pushConstants(m_mesh_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAll, sizeof(uint32_t) * 2, sizeof(uint32_t) * 2, &pc.render_resolution);
		cmd_buffer.pushConstants(m_mesh_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAll, sizeof(uint32_t) * 4, sizeof(float) * 2, &pc.jitter_offset);
	}

	const auto& snapshot = scene.GetSystem<SceneSnapshotSystem>()->GetSnapshotData();
	// Draw all meshes in scene snapshot data
	for (auto& range : snapshot.mesh_ranges) {
		auto mesh_asset = AssetManager::GetAssetRaw<StaticMeshAsset>(range.mesh_uuid);
		auto& mesh_buffer_entry_data = asset_manager.mesh_buffer_manager.GetEntryData(mesh_asset->data.get());

		std::vector<vk::DeviceSize> offsets = {
			mesh_buffer_entry_data.data_start_vertex_idx * sizeof(glm::vec3),
			mesh_buffer_entry_data.data_start_vertex_idx * sizeof(glm::vec3),
			mesh_buffer_entry_data.data_start_vertex_idx * sizeof(glm::vec2),
			mesh_buffer_entry_data.data_start_vertex_idx * sizeof(glm::vec3)
		};

		cmd_buffer.bindVertexBuffers(0, 4, vert_buffers.data(), offsets.data());
		cmd_buffer.bindIndexBuffer(index_buffers[0], mesh_buffer_entry_data.data_start_indices_idx * sizeof(uint32_t), vk::IndexType::eUint32);


		for (uint32_t i = range.start_idx; i < range.start_idx + range.count; i++) {
			pc.transform_idx = snapshot.static_mesh_data[i].transform_buffer_idx;
			cmd_buffer.pushConstants(m_mesh_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAll, 0, sizeof(uint32_t), &pc.transform_idx);

			for (auto& submesh : mesh_asset->data->submeshes) {
				pc.material_idx = (snapshot.static_mesh_data[i].material_vec)[submesh.material_index]->GetGlobalBufferIndex();
				cmd_buffer.pushConstants(m_mesh_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAll, sizeof(uint32_t), sizeof(uint32_t), &pc.material_idx);
				cmd_buffer.drawIndexed(submesh.num_indices, 1, submesh.base_index, submesh.base_vertex, 0);
			}
		}
	}

	// Particles
	cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_particle_pipeline.GetPipeline());
	cmd_buffer.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, m_particle_pipeline.pipeline_layout.GetPipelineLayout(), 0, buffer_indices, buffer_offsets);

	auto& ptcl_mesh = AssetManager::GetAsset<StaticMeshAsset>(AssetManager::CoreAssetIDs::CUBE_MESH).get()->data;
	auto& mesh_buffer_entry_data = asset_manager.mesh_buffer_manager.GetEntryData(ptcl_mesh.get());

	std::vector<vk::DeviceSize> offsets = {
		mesh_buffer_entry_data.data_start_vertex_idx * sizeof(glm::vec3),
		mesh_buffer_entry_data.data_start_vertex_idx * sizeof(glm::vec3),
		mesh_buffer_entry_data.data_start_vertex_idx * sizeof(glm::vec2),
		mesh_buffer_entry_data.data_start_vertex_idx * sizeof(glm::vec3)
	};

	//pc.material_idx = AssetManager::GetAsset<MaterialAsset>(AssetManager::CoreAssetIDs::MATERIAL)->GetGlobalBufferIndex();
	cmd_buffer.bindVertexBuffers(0, 4, vert_buffers.data(), offsets.data());
	cmd_buffer.bindIndexBuffer(index_buffers[0], mesh_buffer_entry_data.data_start_indices_idx * sizeof(uint32_t), vk::IndexType::eUint32);
	cmd_buffer.pushConstants(m_particle_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAll, sizeof(uint32_t), sizeof(uint32_t), &pc.material_idx);
	cmd_buffer.pushConstants(m_particle_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAll, sizeof(uint32_t) * 2, sizeof(uint32_t) * 2, &pc.render_resolution);
	cmd_buffer.pushConstants(m_particle_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAll, sizeof(uint32_t) * 4, sizeof(float) * 2, &pc.jitter_offset);

	auto* p_particle_system = scene.GetSystem<ParticleSystem>();
	cmd_buffer.drawIndexed(ptcl_mesh->submeshes[0].num_indices, p_particle_system->GetParticleCount(), ptcl_mesh->submeshes[0].base_index, ptcl_mesh->submeshes[0].base_vertex, 0);

	cmd_buffer.endRenderingKHR();

}