#include "pch/pch.h"
#include "renderpasses/TAA_ResolvePass.h"

using namespace SNAKE;

void TAA_ResolvePass::Init() {
	m_pipeline.Init("res/shaders/TAA_Resolvecomp_00000000.spv");

	auto spec = mp_output_image->GetSpec();
	spec.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
	m_history_image.SetSpec(spec);
	m_history_image.CreateImage();

	auto velocity_spec = mp_velocity_image->GetSpec();
	velocity_spec.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
	m_prev_frame_velocity_image.SetSpec(velocity_spec);
	m_prev_frame_velocity_image.CreateImage();

	m_history_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone,
		vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

	m_prev_frame_velocity_image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eNone, vk::AccessFlagBits::eNone,
		vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe);

	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_cmd_buffers[i].Init(vk::CommandBufferLevel::ePrimary);

		m_descriptor_buffers[i].SetDescriptorSpec(m_pipeline.pipeline_layout.GetDescriptorSetLayout(0));
		m_descriptor_buffers[i].CreateBuffer(1);
		auto output_image_get_info = mp_output_image->CreateDescriptorGetInfo(vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage);
		auto history_sampler_get_info = m_history_image.CreateDescriptorGetInfo(vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
		auto prev_frame_velocity_sampler_get_info = m_prev_frame_velocity_image.CreateDescriptorGetInfo(vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
		auto velocity_sampler_get_info = mp_velocity_image->CreateDescriptorGetInfo(vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
		m_descriptor_buffers[i].LinkResource(mp_velocity_image, velocity_sampler_get_info, 0, 0);
		m_descriptor_buffers[i].LinkResource(&m_history_image, history_sampler_get_info, 1, 0);
		m_descriptor_buffers[i].LinkResource(&m_prev_frame_velocity_image, prev_frame_velocity_sampler_get_info, 2, 0);
		m_descriptor_buffers[i].LinkResource(mp_output_image, output_image_get_info, 3, 0);
	}
}

vk::CommandBuffer TAA_ResolvePass::RecordCommandBuffer() {
	FrameInFlightIndex fif = VkContext::GetCurrentFIF();

	auto& cmd = *m_cmd_buffers[fif].buf;
	vk::CommandBufferBeginInfo begin_info{};
	SNK_CHECK_VK_RESULT(cmd.begin(begin_info));
	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline.GetPipeline());

	auto binding_infos = util::array(m_descriptor_buffers[fif].GetBindingInfo());
	cmd.bindDescriptorBuffersEXT(binding_infos);
	uint32_t offset = 0u;
	cmd.setDescriptorBufferOffsetsEXT(vk::PipelineBindPoint::eCompute, m_pipeline.pipeline_layout.GetPipelineLayout(), 0u, offset, offset);

	glm::vec2 render_size = mp_output_image->GetSpec().size;
	cmd.dispatch((uint32_t)(render_size.x / 8.f), (uint32_t)(render_size.y / 8.f), 1u);

	mp_output_image->BlitTo(m_history_image, 0, 0, vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eGeneral,
		vk::ImageLayout::eShaderReadOnlyOptimal, vk::Filter::eNearest, std::nullopt, cmd);

	mp_velocity_image->BlitTo(m_prev_frame_velocity_image, 0, 0, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
		vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::Filter::eNearest, std::nullopt, cmd);

	SNK_CHECK_VK_RESULT(cmd.end());

	return cmd;
}