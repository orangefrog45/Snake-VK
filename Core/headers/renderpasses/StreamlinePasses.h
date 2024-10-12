#pragma once
#include "core/VkIncl.h"
#include "rendering/StreamlineWrapper.h"

namespace SNAKE {
	class Image2D;

	class StreamlinePasses {
	public:
		void Init();

		void SetDLSS_Options(const sl::DLSSOptions& options);

		void EvaluateDLSS(vk::CommandBuffer cmd, struct GBufferResources& gbuffer, Image2D& colour_in, Image2D& colour_out);

	private:
		sl::ResourceTag m_colour_in_tag;
		sl::ResourceTag m_colour_out_tag;
		sl::ResourceTag m_depth_tag;
		sl::ResourceTag m_mvec_tag;
	};
}