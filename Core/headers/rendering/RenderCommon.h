#pragma once
#include "resources/Images.h"

namespace SNAKE {
	struct GBufferResources {
		Image2D albedo_image;
		Image2D normal_image;
		Image2D depth_image;
		Image2D rma_image;
		Image2D pixel_motion_image;
	};


	inline static float Halton(unsigned frame_idx, unsigned base) {
		float f = 1.f;
		float r = 0.f;

		while (frame_idx > 0) {
			f *= (float)base;
			r += (float)(frame_idx % base) / f;
			frame_idx /= base;
		}
		return r;
	}

	inline static glm::vec2 GetFrameJitter(unsigned frame_idx, unsigned internal_render_width, unsigned output_render_width) {
		float scale_ratio = (float)output_render_width / (float)internal_render_width;
		unsigned jitter_phases = (unsigned)(8 * scale_ratio * scale_ratio + 0.5f);
		unsigned jitter_current_idx = (frame_idx % jitter_phases) + 1;

		return { Halton(jitter_current_idx, 2) - 0.5f, Halton(jitter_current_idx, 3) - 0.5f };
	}
}