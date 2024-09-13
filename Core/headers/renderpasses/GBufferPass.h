#pragma once
#include "assets/AssetManager.h"
#include "core/DescriptorBuffer.h"
#include "core/Pipelines.h"
#include "core/VkCommands.h"
#include "rendering/RenderCommon.h"
#include "scene/Scene.h"
#include "scene/SceneInfoBufferSystem.h"
#include "scene/TransformBufferSystem.h"
#include "scene/SceneSnapshotSystem.h"

namespace SNAKE {

	class GBufferPass {
	public:
		struct GBufferPC {
			uint32_t transform_idx;
			uint32_t material_idx;
		};

	public:
		// output - resources which will be rendered to with RecordCommandBuffer
		void Init(Scene& scene, GBufferResources& output) {
			auto binding_desc = Vertex::GetBindingDescription();
			auto attribute_desc = Vertex::GetAttributeDescriptions();

			GraphicsPipelineBuilder gp_builder{};
			gp_builder.AddColourAttachment(output.albedo_image.GetSpec().format)
				.AddColourAttachment(output.normal_image.GetSpec().format)
				.AddColourAttachment(output.rma_image.GetSpec().format)
				.AddDepthAttachment(output.depth_image.GetSpec().format)
				.AddShader(vk::ShaderStageFlagBits::eVertex, "res/shaders/GBuffervert_00000000.spv")
				.AddShader(vk::ShaderStageFlagBits::eFragment, "res/shaders/GBufferfrag_00000000.spv")
				.AddVertexBinding(attribute_desc[0], binding_desc[0])
				.AddVertexBinding(attribute_desc[1], binding_desc[1])
				.AddVertexBinding(attribute_desc[2], binding_desc[2])
				.AddVertexBinding(attribute_desc[3], binding_desc[3])
				.Build();

			m_pipeline.Init(gp_builder);

			for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				m_descriptor_buffers[i].SetDescriptorSpec(m_pipeline.pipeline_layout.GetDescriptorSetLayout(2));
				m_descriptor_buffers[i].CreateBuffer(1);
				auto* p_transform_system = scene.GetSystem<TransformBufferSystem>();
				auto transform_get_info = p_transform_system->GetTransformStorageBuffer(i).CreateDescriptorGetInfo();
				m_descriptor_buffers[i].LinkResource(&p_transform_system->GetTransformStorageBuffer(i), transform_get_info, 0, 0);

				m_cmd_buffers[i].Init(vk::CommandBufferLevel::ePrimary);
			}
		}

		vk::CommandBuffer RecordCommandBuffer(GBufferResources& output, Scene& scene) {
			auto frame_idx = VkContext::GetCurrentFIF();

			auto cmd_buffer = *m_cmd_buffers[frame_idx].buf;
			SNK_CHECK_VK_RESULT(cmd_buffer.reset());
			vk::CommandBufferBeginInfo begin_info{};
			SNK_CHECK_VK_RESULT(
				m_cmd_buffers[frame_idx].buf->begin(&begin_info)
			);

			output.albedo_image.TransitionImageLayout(vk::ImageLayout::eUndefined,
				vk::ImageLayout::eColorAttachmentOptimal, 0, 1, cmd_buffer);
			output.normal_image.TransitionImageLayout(vk::ImageLayout::eUndefined,
				vk::ImageLayout::eColorAttachmentOptimal, 0, 1, cmd_buffer);

			vk::RenderingAttachmentInfo albedo_attachment_info{};
			albedo_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
			albedo_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
			albedo_attachment_info.imageView = output.albedo_image.GetImageView();
			albedo_attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;

			vk::RenderingAttachmentInfo normal_attachment_info{};
			normal_attachment_info.loadOp = vk::AttachmentLoadOp::eClear; 
			normal_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
			normal_attachment_info.imageView = output.normal_image.GetImageView();
			normal_attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;

			vk::RenderingAttachmentInfo rma_attachment_info{};
			rma_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
			rma_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
			rma_attachment_info.imageView = output.rma_image.GetImageView();
			rma_attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;

			vk::RenderingAttachmentInfo depth_attachment_info{};
			depth_attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
			depth_attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
			depth_attachment_info.imageView = output.depth_image.GetImageView();
			depth_attachment_info.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
			depth_attachment_info.clearValue = vk::ClearValue{ vk::ClearDepthStencilValue{1.f} };

			auto colour_attachments = util::array(albedo_attachment_info, normal_attachment_info, rma_attachment_info);

			vk::RenderingInfo render_info{};
			render_info.layerCount = 1;
			render_info.colorAttachmentCount = colour_attachments.size();
			render_info.pColorAttachments = colour_attachments.data();
			render_info.pDepthAttachment = &depth_attachment_info;
			auto& spec = output.depth_image.GetSpec();
			render_info.renderArea.extent = vk::Extent2D{ spec.size.x, spec.size.y };
			render_info.renderArea.offset = vk::Offset2D{ 0, 0 };

			cmd_buffer.beginRenderingKHR(
				render_info
			);

			// Binds shaders and sets all state like rasterizer, blend, multisampling etc
			cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.GetPipeline());

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

			auto* p_transform_buffer_system = scene.GetSystem<TransformBufferSystem>();
			auto* p_scene_info_buffer_sys = scene.GetSystem<SceneInfoBufferSystem>();

			std::array<vk::DescriptorBufferBindingInfoEXT, 3> binding_infos = { 
				p_scene_info_buffer_sys->descriptor_buffers[frame_idx].GetBindingInfo(),
				AssetManager::GetGlobalTexMatBufBindingInfo(frame_idx),
				m_descriptor_buffers[VkContext::GetCurrentFIF()].GetBindingInfo()
			};

			cmd_buffer.bindDescriptorBuffersEXT(binding_infos);

			std::array<uint32_t, 3> buffer_indices = { 0, 1, 2 };
			std::array<vk::DeviceSize, 3> buffer_offsets = { 0, 0, 0 };

			cmd_buffer.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eGraphics, m_pipeline.pipeline_layout.GetPipelineLayout(), 0, buffer_indices, buffer_offsets);

			auto& asset_manager = AssetManager::Get();
			auto buffers = asset_manager.mesh_buffer_manager.GetMeshBuffers();

			std::vector<vk::Buffer> vert_buffers = { buffers.position_buf.buffer, buffers.normal_buf.buffer, buffers.tex_coord_buf.buffer, buffers.tangent_buf.buffer };
			std::vector<vk::Buffer> index_buffers = { buffers.indices_buf.buffer };

			const auto& snapshot = scene.GetSystem<SceneSnapshotSystem>()->GetSnapshotData();

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

				GBufferPC pc;

				for (uint32_t i = range.start_idx; i < range.start_idx + range.count; i++) {
					pc.transform_idx = snapshot.static_mesh_data[i].transform_buffer_idx;
					cmd_buffer.pushConstants(m_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAllGraphics, 0, sizeof(uint32_t), &pc.transform_idx);

					for (auto& submesh : mesh_asset->data->submeshes) {
						pc.material_idx = (*snapshot.static_mesh_data[i].material_vec)[submesh.material_index]->GetGlobalBufferIndex();
						cmd_buffer.pushConstants(m_pipeline.pipeline_layout.GetPipelineLayout(), vk::ShaderStageFlagBits::eAllGraphics, sizeof(uint32_t), sizeof(uint32_t), &pc.material_idx);
						cmd_buffer.drawIndexed(submesh.num_indices, 1, submesh.base_index, submesh.base_vertex, 0);
					}
				}
			}

			cmd_buffer.endRenderingKHR();

			SNK_CHECK_VK_RESULT(
				cmd_buffer.end()
			);

			return cmd_buffer;
		}

	private:
		GraphicsPipeline m_pipeline;
		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> m_descriptor_buffers;
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;
	};
}