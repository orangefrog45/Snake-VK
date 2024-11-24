#pragma once
#include "resources/Images.h"

namespace SNAKE {
	struct GBufferResources {
		// 8-bit colour
		Image2D albedo_image;

		// 16-bit normals
		Image2D normal_image;

		// 32-bit depth
		Image2D depth_image;

		// 16-bit RMA
		Image2D rma_image;

		// 16-bit motion vectors
		Image2D pixel_motion_image;

		// 16-bit material flags
		Image2D mat_flag_image;
	};

	struct RaytracingResources : public GBufferResources {
		// 16-bit RGBA image
		Image2D reservoir_image;
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
		if (internal_render_width == 0 || output_render_width == 0)
			return { 0, 0 };

		float scale_ratio = (float)output_render_width / (float)internal_render_width;
		unsigned jitter_phases = (unsigned)(8 * scale_ratio * scale_ratio + 0.5f);
		unsigned jitter_current_idx = (frame_idx % jitter_phases) + 1;

		return { Halton(jitter_current_idx, 2) - 0.5f, Halton(jitter_current_idx, 3) - 0.5f };
	}
}