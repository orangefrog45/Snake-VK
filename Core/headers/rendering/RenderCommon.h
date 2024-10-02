#pragma once
#include "resources/Images.h"

namespace SNAKE {
	struct GBufferResources {
		FullscreenImage2D albedo_image{ vk::ImageLayout::eColorAttachmentOptimal };
		FullscreenImage2D normal_image{ vk::ImageLayout::eColorAttachmentOptimal };
		FullscreenImage2D depth_image{ vk::ImageLayout::eDepthAttachmentOptimal };
		FullscreenImage2D rma_image{ vk::ImageLayout::eColorAttachmentOptimal };
		FullscreenImage2D pixel_motion_image{ vk::ImageLayout::eColorAttachmentOptimal };
	};
}