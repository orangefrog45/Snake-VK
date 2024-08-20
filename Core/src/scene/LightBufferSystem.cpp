#include "pch/pch.h"
#include "scene/LightBufferSystem.h"
#include "scene/Scene.h"
#include "util/ExtraMath.h"
#include "util/util.h"
#include "scene/CameraSystem.h"
#include "components/CameraComponent.h"
#include "scene/Entity.h"

namespace SNAKE {

	constexpr size_t directional_light_shader_size = 2 * sizeof(glm::vec4) + sizeof(glm::mat4);
	constexpr size_t pointlight_shader_size = 3 * sizeof(glm::vec4);
	constexpr size_t spotlight_shader_size = 4 * sizeof(glm::vec4);
	constexpr size_t light_array_size = 1024;
	constexpr size_t total_light_ssbo_size = directional_light_shader_size + (pointlight_shader_size + spotlight_shader_size) * light_array_size;

	void LightBufferSystem::OnSystemAdd() {
		m_frame_start_listener.callback = [this]([[maybe_unused]] auto _event ) {
			UpdateLightSSBO();
			};

		EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);

		for (FrameInFlightIndex i = 0; i < light_ssbos.size(); i++) {
			light_ssbos[i].CreateBuffer(total_light_ssbo_size, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
				VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

			auto descriptor_spec = std::make_shared<DescriptorSetSpec>();
			descriptor_spec->AddDescriptor(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eAllGraphics)
				.GenDescriptorLayout();

			light_descriptor_buffers[i].SetDescriptorSpec(descriptor_spec);
			light_descriptor_buffers[i].CreateBuffer(1);
			auto storage_buffer_info = light_ssbos[i].CreateDescriptorGetInfo();
			light_descriptor_buffers[i].LinkResource(&storage_buffer_info.first, 0, 0);
		}
	}

	struct DirLightData {
		glm::vec4 colour;
		glm::vec4 dir;
		glm::mat4 transform;
	};

	struct PointlightData {
		glm::vec4 colour;
		glm::vec4 pos;
		float atten_exp;
		float atten_linear;
		float atten_constant;
	};

	struct SpotlightData  {
		glm::vec4 colour;
		glm::vec4 pos;
		float atten_exp;
		float atten_linear;
		float atten_constant;
		float aperture;
	};


	void LightBufferSystem::UpdateLightSSBO() {
		auto frame_idx = VulkanContext::GetCurrentFIF();
		size_t current_shader_offset = 0;
		std::byte* p_data = reinterpret_cast<std::byte*>(light_ssbos[frame_idx].Map());

		DirLightData dir_data;
		dir_data.colour = glm::vec4(p_scene->directional_light.colour, 1.f);
		dir_data.dir = glm::vec4(glm::normalize(ExtraMath::SphericalToCartesian(1.f, p_scene->directional_light.spherical_coords.x, p_scene->directional_light.spherical_coords.y)), 1.f);
		auto ortho = glm::ortho(-100.f, 100.f, -100.f, 100.f, 0.1f, 100.f);
		auto view = glm::lookAt(glm::vec3(dir_data.dir) * 50.f, glm::vec3(0), glm::vec3{0, 1, 0});
		dir_data.transform = ortho * view;
		memcpy(light_ssbos[frame_idx].Map(), &dir_data, directional_light_shader_size);
		current_shader_offset += directional_light_shader_size;

		auto pointlight_view = p_scene->GetRegistry().view<PointlightComponent>();
		auto spotlight_view = p_scene->GetRegistry().view<SpotlightComponent>();
		std::array<uint32_t, 2> light_counts = { (uint32_t)pointlight_view.size(), (uint32_t)spotlight_view.size() };
		memcpy(p_data + current_shader_offset, light_counts.data(), light_counts.size() * sizeof(uint32_t));
		current_shader_offset += 4 * sizeof(uint32_t);

		std::vector<std::byte> light_data_vec{ pointlight_view.size() * pointlight_shader_size };
		std::byte* p_light_vec = light_data_vec.data();
		for (auto [entity, pointlight] : pointlight_view.each()) {
			util::ConvertToBytes(p_light_vec, glm::vec4(pointlight.colour, 1.f));
			util::ConvertToBytes(p_light_vec, glm::vec4(p_scene->GetRegistry().get<TransformComponent>(entity).GetAbsPosition(), 1.f));
			util::ConvertToBytes(p_light_vec, glm::vec4(pointlight.attenuation.exp, pointlight.attenuation.linear, pointlight.attenuation.constant, 1.f));
		}
		memcpy(p_data + current_shader_offset, light_data_vec.data(), light_data_vec.size());
		current_shader_offset += pointlight_shader_size * light_array_size;


		light_data_vec.resize(spotlight_shader_size * spotlight_view.size());
		p_light_vec = light_data_vec.data();
		for (auto [entity, spotlight] : spotlight_view.each()) {
			util::ConvertToBytes(p_light_vec, glm::vec4(spotlight.colour, 1.f));
			auto& transform = p_scene->GetRegistry().get<TransformComponent>(entity);
			util::ConvertToBytes(p_light_vec, glm::vec4(transform.GetAbsPosition(), 1.f));
			util::ConvertToBytes(p_light_vec, glm::vec4(transform.forward, 1.f));
			util::ConvertToBytes(p_light_vec, glm::vec4(spotlight.attenuation.exp, spotlight.attenuation.linear, spotlight.attenuation.constant, 1.f));
		}
		memcpy(p_data, light_data_vec.data(), light_data_vec.size());

	
	}
}