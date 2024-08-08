#pragma once
#include "System.h"
#include "core/VkCommands.h"
#include "core/DescriptorBuffer.h"
#include "components/Lights.h"
#include "scene/Scene.h"


namespace SNAKE {
	class LightBufferSystem : public System {
	public:
		void OnSystemAdd();

		struct LightSSBO {
			alignas(16) glm::vec4 colour;
			alignas(16) glm::vec4 dir;
			alignas(16) glm::mat4 light_transform;
		};

		void UpdateLightSSBO(FrameInFlightIndex frame_idx) {
			auto* p_data = light_ssbos[frame_idx].Map();

			LightSSBO light_ubo;
			light_ubo.colour = glm::vec4(1, 0, 0, 1);
			light_ubo.dir = glm::vec4(1, 0, 0, 1);

			auto ortho = glm::ortho(-10.f, 10.f, -10.f, 10.f, 0.1f, 10.f);
			auto view = glm::lookAt(glm::vec3{ -5, 0, 0 }, glm::vec3{ 0, 0, 0 }, glm::vec3{ 0, 1, 0 });
			light_ubo.light_transform = ortho * view;

			memcpy(light_ssbos[frame_idx].Map(), &light_ubo, sizeof(LightSSBO));
		}

		void OnUpdate() override {
		}

		std::array<DescriptorBuffer, MAX_FRAMES_IN_FLIGHT> light_descriptor_buffers;
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> light_ssbos;
	private:
		EventListener m_frame_start_listener;

	};
}