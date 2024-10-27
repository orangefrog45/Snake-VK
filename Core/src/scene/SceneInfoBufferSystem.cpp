#include "pch/pch.h"
#include "components/CameraComponent.h"
#include "scene/CameraSystem.h"
#include "scene/Scene.h"
#include "scene/SceneInfoBufferSystem.h"
#include "core/VkContext.h"
#include "core/VkCommon.h"

using namespace SNAKE;

void SceneInfoBufferSystem::OnSystemAdd() {
	constexpr size_t UBO_SIZE = aligned_size(sizeof(glm::mat4) * 2 + sizeof(glm::vec4) * 2 + sizeof(unsigned), 64);

	m_frame_start_listener.callback = [this]([[maybe_unused]] auto _event) {
		uint8_t fif = VkContext::GetCurrentFIF();
		UpdateUBO(fif);
		uint8_t old_idx = (uint8_t)glm::min(fif - 1u, MAX_FRAMES_IN_FLIGHT - 1u);

		// Copy last frames data into new frames "old" buffer
		CopyBuffer(m_ubos[old_idx].buffer, m_old_ubos[fif].buffer, UBO_SIZE);
		};

	EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);

	mp_descriptor_set_spec = std::make_shared<DescriptorSetSpec>();
	mp_descriptor_set_spec->AddDescriptor(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAll) // Current buffer
		.AddDescriptor(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eAll) // Old buffer
		.GenDescriptorLayout();

	for (FrameInFlightIndex i = 0; i < m_ubos.size(); i++) {
		m_ubos[i].CreateBuffer(UBO_SIZE, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferSrc,
			VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
		m_old_ubos[i].CreateBuffer(UBO_SIZE, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
			VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
		
		descriptor_buffers[i].SetDescriptorSpec(mp_descriptor_set_spec);
		descriptor_buffers[i].CreateBuffer(1);
		auto storage_buffer_info = m_ubos[i].CreateDescriptorGetInfo();
		auto prev_frame_storage_buffer_info = m_old_ubos[i].CreateDescriptorGetInfo();
		descriptor_buffers[i].LinkResource(&m_ubos[i], storage_buffer_info, 0, 0);
		descriptor_buffers[i].LinkResource(&m_old_ubos[i], prev_frame_storage_buffer_info, 1, 0);
	}
}

struct CommonUBO {
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
	alignas(16) glm::mat4 proj_view;
	alignas(16) glm::vec4 cam_pos;
	alignas(16) glm::vec4 cam_forward;
	alignas(16) uint32_t frame_idx;
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
	ubo.frame_idx = VkContext::GetCurrentFrameIdx();
	ubo.view = glm::lookAt(p_transform->GetPosition(), p_transform->GetPosition() + p_transform->forward, glm::vec3(0.f, 1.f, 0.f));
	ubo.proj = p_cam_comp->GetProjectionMatrix();
	ubo.proj_view = ubo.proj * ubo.view;

	auto* p_cam_transform = p_scene->GetSystem<CameraSystem>()->GetActiveCam()->GetEntity()->GetComponent<TransformComponent>();
	ubo.cam_pos = glm::vec4(p_cam_transform->GetAbsPosition(), 1.f);
	ubo.cam_forward = glm::vec4(p_cam_transform->forward, 1.f);

	memcpy(m_ubos[frame_idx].Map(), &ubo, sizeof(CommonUBO));
}
