#pragma once
#include "resources/Images.h"

namespace SNAKE {
	struct GBufferResources {
		FullscreenImage2D albedo_image;
		FullscreenImage2D normal_image;
		FullscreenImage2D depth_image;
		FullscreenImage2D rma_image;
	};
}