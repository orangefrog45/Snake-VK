#include "pch/pch.h"
#include "components/CameraComponent.h"
#include "scene/CameraSystem.h"
#include "scene/Scene.h"
#include "scene/SceneInfoBufferSystem.h"
#include "core/Frametiming.h"
#include "core/VkContext.h"
#include "core/VkCommon.h"

using namespace SNAKE;


struct CommonUBO {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 proj_view;
	glm::vec4 cam_pos;
	glm::vec4 cam_forward;
	uint32_t frame_idx;
	float app_time_elapsed;
	float delta_time;
};

static CommonUBO s_ubo{};

void SceneInfoBufferSystem::OnSystemAdd() {
	constexpr size_t UBO_SIZE = aligned_size(sizeof(CommonUBO), 64);
	m_frame_start_listener.callback = [this]([[maybe_unused]] auto _event) {
		uint8_t fif = VkContext::GetCurrentFIF();
		memcpy(m_old_ubos[fif].Map(), &s_ubo, sizeof(CommonUBO));
		UpdateUBO(fif);
	};

	EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);

	mp_descriptor_set_spec = std::make_shared<DescriptorSetSpec>();
	mp_descriptor_set_spec->AddDescriptor(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAll) // Current buffer
		.AddDescriptor(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAll) // Old buffer
		.GenDescriptorLayout();

	for (FrameInFlightIndex i = 0; i < m_ubos.size(); i++) {
		m_ubos[i].CreateBuffer(UBO_SIZE, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
		m_old_ubos[i].CreateBuffer(UBO_SIZE, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
		
		descriptor_buffers[i].SetDescriptorSpec(mp_descriptor_set_spec);
		descriptor_buffers[i].CreateBuffer(1);
		auto storage_buffer_info = m_ubos[i].CreateDescriptorGetInfo();
		auto prev_frame_storage_buffer_info = m_old_ubos[i].CreateDescriptorGetInfo();
		descriptor_buffers[i].LinkResource(&m_ubos[i], storage_buffer_info, 0, 0);
		descriptor_buffers[i].LinkResource(&m_old_ubos[i], prev_frame_storage_buffer_info, 1, 0);

		m_cmd_buffers[i].Init(vk::CommandBufferLevel::ePrimary);
	}
}



void SceneInfoBufferSystem::UpdateUBO(FrameInFlightIndex frame_idx) {
	auto* p_cam_sys = p_scene->GetSystem<CameraSystem>();
	if (!p_cam_sys)
		return;

	auto* p_cam_comp = p_cam_sys->GetActiveCam();
	if (!p_cam_comp)
		return;

	auto* p_cam_ent = p_cam_comp->GetEntity();
	auto* p_transform = p_cam_ent->GetComponent<TransformComponent>();

	s_ubo.frame_idx = VkContext::GetCurrentFrameIdx();
	s_ubo.view = glm::lookAt(p_transform->GetPosition(), p_transform->GetPosition() + p_transform->forward, glm::vec3(0.f, 1.f, 0.f));
	s_ubo.proj = p_cam_comp->GetProjectionMatrix();
	s_ubo.proj_view = s_ubo.proj * s_ubo.view;
	s_ubo.app_time_elapsed = FrameTiming::GetTotalElapsedTime();
	s_ubo.delta_time = FrameTiming::GetTimeStep();

	auto* p_cam_transform = p_scene->GetSystem<CameraSystem>()->GetActiveCam()->GetEntity()->GetComponent<TransformComponent>();
	s_ubo.cam_pos = glm::vec4(p_cam_transform->GetAbsPosition(), 1.f);
	s_ubo.cam_forward = glm::vec4(p_cam_transform->forward, 1.f);

	memcpy(m_ubos[frame_idx].Map(), &s_ubo, sizeof(CommonUBO));
}
