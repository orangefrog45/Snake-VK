#include "renderpasses/StreamlinePasses.h"
#include "rendering/StreamlineWrapper.h"
#include "rendering/RenderCommon.h"
#include "core/VkCommands.h"


using namespace SNAKE;

void StreamlinePasses::Init() {



}

void StreamlinePasses::SetDLSS_Options(const sl::DLSSOptions& options) {
	if (SL_FAILED(res, slDLSSSetOptions({}, options))) {
		SNK_CORE_CRITICAL("Error setting DLSS options, err: {}", (unsigned)res);
	}
}

void StreamlinePasses::EvaluateDLSS(vk::CommandBuffer cmd, GBufferResources& gbuffer, Image2D& colour_in, Image2D& colour_out) {
	sl::Resource res_colour_in{ sl::ResourceType::eTex2d, colour_in.GetImage(), colour_in.GetMemory(), colour_in.GetImageView(), (uint32_t)vk::ImageLayout::eGeneral };
	sl::Resource res_colour_out{ sl::ResourceType::eTex2d, colour_out.GetImage(), colour_out.GetMemory(), colour_out.GetImageView(), (uint32_t)vk::ImageLayout::eColorAttachmentOptimal };
	sl::Resource res_depth{ sl::ResourceType::eTex2d, gbuffer.depth_image.GetImage(),  gbuffer.depth_image.GetMemory(),  gbuffer.depth_image.GetImageView(), (uint32_t)vk::ImageLayout::eDepthAttachmentOptimal };
	sl::Resource res_mvec{ sl::ResourceType::eTex2d, gbuffer.pixel_motion_image.GetImage(), gbuffer.pixel_motion_image.GetMemory(), gbuffer.pixel_motion_image.GetImageView(), (uint32_t)vk::ImageLayout::eColorAttachmentOptimal };

	glm::uvec2 extent_input = gbuffer.depth_image.GetSpec().size;
	glm::uvec2 extent_output = colour_out.GetSpec().size;

	res_colour_in.width = extent_input.x;
	res_colour_in.height = extent_input.y;
	res_colour_in.nativeFormat = (uint32_t)colour_in.GetSpec().format;
	res_colour_in.mipLevels = 1;
	res_colour_in.arrayLayers = 1;
	res_colour_in.usage = (uint32_t)colour_in.GetSpec().usage;

	res_colour_out.width = extent_output.x;
	res_colour_out.height = extent_output.y;
	res_colour_out.nativeFormat = (uint32_t)colour_out.GetSpec().format;
	res_colour_out.mipLevels = 1;
	res_colour_out.arrayLayers = 1;
	res_colour_out.usage = (uint32_t)colour_out.GetSpec().usage;

	res_depth.width = extent_input.x;
	res_depth.height = extent_input.y;
	res_depth.nativeFormat = (uint32_t)gbuffer.depth_image.GetSpec().format;
	res_depth.mipLevels = 1;
	res_depth.arrayLayers = 1;
	res_depth.usage = (uint32_t)gbuffer.depth_image.GetSpec().usage;

	res_mvec.width = extent_input.x;
	res_mvec.height = extent_input.y;
	res_mvec.nativeFormat = (uint32_t)gbuffer.pixel_motion_image.GetSpec().format;
	res_mvec.mipLevels = 1;
	res_mvec.arrayLayers = 1;
	res_mvec.usage = (uint32_t)gbuffer.pixel_motion_image.GetSpec().usage;

	sl::Extent sl_extent_inputs{};
	sl_extent_inputs.left = 0;
	sl_extent_inputs.top = 0;
	sl_extent_inputs.height = extent_input.y;
	sl_extent_inputs.width = extent_input.x;

	sl::Extent sl_extent_output{};
	sl_extent_output.left = 0;
	sl_extent_output.top = 0;
	sl_extent_output.height = extent_output.y;
	sl_extent_output.width = extent_output.x;

	m_colour_in_tag = { &res_colour_in, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilPresent, &sl_extent_inputs };
	m_colour_out_tag = { &res_colour_out, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilPresent, &sl_extent_output };
	m_depth_tag = { &res_depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &sl_extent_inputs };
	m_mvec_tag = { &res_mvec, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, &sl_extent_inputs };

	auto tags = util::array(m_colour_in_tag, m_colour_out_tag, m_depth_tag, m_mvec_tag);
	if (SL_FAILED(tag_res, slSetTag({}, tags.data(), (uint32_t)tags.size(), cmd))) {
		SNK_CORE_CRITICAL("Error setting DLSS resource tags, err: {}", (unsigned)tag_res);
	}
	sl::ViewportHandle vh{};

	const sl::BaseStructure* inputs[] = { &vh };

	if (SL_FAILED(res, slEvaluateFeature(sl::kFeatureDLSS, *StreamlineWrapper::GetCurrentFrameToken(), inputs, 1, cmd))) {
		SNK_CORE_ERROR("DLSS evaluation failed, err: {}", (uint32_t)res);
	}
}


