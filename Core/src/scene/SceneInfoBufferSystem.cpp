#include "pch/pch.h"
#include "scene/SceneInfoBufferSystem.h"
#include "scene/CameraSystem.h"
#include "components/CameraComponent.h"
#include "scene/Scene.h"

namespace SNAKE {
	void SceneInfoBufferSystem::OnSystemAdd() {
		constexpr size_t UBO_SIZE = aligned_size(sizeof(glm::mat4) * 2 + sizeof(glm::vec4), 64);

		m_frame_start_listener.callback = [this](Event const* p_event) {
			auto* p_casted = dynamic_cast<FrameStartEvent const*>(p_event);
			UpdateUBO(p_casted->frame_flight_index);
			};

		EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);

		for (FrameInFlightIndex i = 0; i < ubos.size(); i++) {
			ubos[i].CreateBuffer(UBO_SIZE, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
				VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

			auto descriptor_spec = std::make_shared<DescriptorSetSpec>();
			descriptor_spec->AddDescriptor(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAllGraphics)
				.GenDescriptorLayout();

			descriptor_buffers[i].SetDescriptorSpec(descriptor_spec);
			descriptor_buffers[i].CreateBuffer(1);
			auto storage_buffer_info = ubos[i].CreateDescriptorGetInfo();
			descriptor_buffers[i].LinkResource(storage_buffer_info.first, 0, 0);
		}
	}

	struct CommonUBO {
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
		alignas(16) glm::vec4 cam_pos;
	};


	void SceneInfoBufferSystem::UpdateUBO(FrameInFlightIndex frame_idx) {
		auto* p_cam_sys = p_scene->GetSystem<CameraSystem>();
		if (!p_cam_sys)
			return;

		auto* p_cam_comp = p_cam_sys->GetActiveCam();
		if (!p_cam_comp)
			return;

		auto* p_cam_ent = p_cam_comp->GetEntity();
		auto* p_transform = p_cam_ent->GetComponent<TransformComponent>();

		CommonUBO ubo{};
		auto p = p_transform->GetPosition();

		ubo.view = glm::lookAt(p_transform->GetPosition(), p_transform->GetPosition() + p_transform->forward, glm::vec3(0.f, 1.f, 0.f));
		ubo.proj = p_cam_comp->GetProjectionMatrix();
		ubo.cam_pos = glm::vec4(p_scene->GetSystem<CameraSystem>()->GetActiveCam()->GetEntity()->GetComponent<TransformComponent>()->GetAbsPosition(), 1.f);

		// Y coordinate of clip coordinates inverted as glm designed to work with opengl, flip here
		ubo.proj[1][1] *= -1;

		memcpy(ubos[frame_idx].Map(), &ubo, sizeof(CommonUBO));
	}
}