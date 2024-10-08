#pragma once
#include "renderpasses/ForwardPass.h"
#include "renderpasses/ShadowPass.h"
#include "RenderCommon.h"

namespace SNAKE {
	class VkSceneRenderer {
	public:
		void Init(class Scene* p_scene);

		// Renders scene and stores result in output_image
		// Forward pass will wait on forward_wait_semphore if provided
		void RenderScene(Image2D& output_image, Image2D& depth_image, const struct SceneSnapshotData& snapshot_data);

		// Sets scene to render
		void SetScene(Scene* p_scene) {
			mp_scene = p_scene;
		}

	private:
		Scene* mp_scene = nullptr;

		ForwardPass m_forward_pass;
		ShadowPass m_shadow_pass;
	};
}