#include "pch/pch.h"
#include "assets/ResourceBufferManagers.h"
#include "assets/AssetManager.h"

using namespace SNAKE;

void GlobalMaterialBufferManager::Init(const std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT>& buffers) {
	descriptor_buffers = buffers;
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_material_ubos[i].CreateBuffer(4096 * material_size,
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

		auto info = m_material_ubos[i].CreateDescriptorGetInfo();
		descriptor_buffers[i]->LinkResource(&m_material_ubos[i], info, 0, 0);
	}

	m_material_update_event_listener.callback = [this](Event const* p_event) {
		auto const* p_casted = dynamic_cast<MaterialAsset::MaterialUpdateEvent const*>(p_event);

		for (uint8_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			m_materials_to_update[i].push_back(p_casted->p_material);
		}
		};

	m_frame_start_listener.callback = [this]([[maybe_unused]] Event const* p_event) {
		UpdateMaterialUBO();
		};

	m_material_asset_event_listener.callback = [this](Event const* p_event) {
		auto* p_casted = dynamic_cast<AssetEvent const*>(p_event);
		if (auto* p_mat = dynamic_cast<MaterialAsset*>(p_casted->p_asset)) {
			RegisterMaterial(p_mat);
		}
	};

	EventManagerG::RegisterListener<MaterialAsset::MaterialUpdateEvent>(m_material_update_event_listener);
	EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);
	EventManagerG::RegisterListener<MaterialAsset>(m_material_asset_event_listener);
}

void GlobalMaterialBufferManager::RegisterMaterial(AssetRef<MaterialAsset> material) {
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_materials_to_register[i].push_back(material);
		m_materials_to_update[i].push_back(material);
	}
	material->m_global_buffer_index = m_current_index++;
}

void GlobalMaterialBufferManager::UpdateMaterialUBO() {
	auto frame_in_flight_idx = VkContext::GetCurrentFIF();

	std::byte* p_data = reinterpret_cast<std::byte*>(m_material_ubos[frame_in_flight_idx].Map());

	for (auto& mat_ref : m_materials_to_update[frame_in_flight_idx]) {
		SNK_ASSERT(mat_ref->GetGlobalBufferIndex() != MaterialAsset::INVALID_GLOBAL_INDEX);

		std::array<std::byte, material_size> data;

		unsigned idx = 0;

		idx = mat_ref->albedo_tex ? mat_ref->albedo_tex->GetGlobalIndex() : Texture2DAsset::INVALID_GLOBAL_INDEX;
		memcpy(data.data(), &idx, sizeof(unsigned));
		idx = mat_ref->normal_tex ? mat_ref->normal_tex->GetGlobalIndex() : Texture2DAsset::INVALID_GLOBAL_INDEX;
		memcpy(data.data() + sizeof(unsigned), &idx, sizeof(unsigned));
		idx = mat_ref->roughness_tex ? mat_ref->roughness_tex->GetGlobalIndex() : Texture2DAsset::INVALID_GLOBAL_INDEX;
		memcpy(data.data() + sizeof(unsigned) * 2, &idx, sizeof(unsigned));
		idx = mat_ref->metallic_tex ? mat_ref->metallic_tex->GetGlobalIndex() : Texture2DAsset::INVALID_GLOBAL_INDEX;
		memcpy(data.data() + sizeof(unsigned) * 3, &idx, sizeof(unsigned));
		idx = mat_ref->ao_tex ? mat_ref->ao_tex->GetGlobalIndex() : Texture2DAsset::INVALID_GLOBAL_INDEX;
		memcpy(data.data() + sizeof(unsigned) * 4, &idx, sizeof(unsigned));

		std::array<float, 7> params = { mat_ref->roughness, mat_ref->metallic, mat_ref->ao, mat_ref->albedo.r, mat_ref->albedo.g, mat_ref->albedo.b, 1.f };
		memcpy(data.data() + sizeof(unsigned) * 5, &params, sizeof(float) * 7);

		memcpy(p_data + mat_ref->GetGlobalBufferIndex() * material_size, data.data(), data.size());
	}

	m_materials_to_update[frame_in_flight_idx].clear();
}


void GlobalTextureBufferManager::Init(const std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT>& buffers) {
	descriptor_buffers = buffers;

	m_frame_start_listener.callback = [this]([[maybe_unused]] Event const* p_event) {
		RegisterTexturesInternal();
		};

	EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);
}

void GlobalTextureBufferManager::RegisterTexture(AssetRef<Texture2DAsset> tex) {
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_textures_to_register[i].push_back(tex);
	}
	tex->m_global_index = m_current_index++;
}

void GlobalTextureBufferManager::RegisterTexturesInternal() {
	auto frame_in_flight_idx = VkContext::GetCurrentFIF();

	for (auto& tex : m_textures_to_register[frame_in_flight_idx]) {
		auto info = tex->image.CreateDescriptorGetInfo(vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler);
		descriptor_buffers[frame_in_flight_idx]->LinkResource(&tex->image, info, 1, 0, tex->GetGlobalIndex());
		
	}

	m_textures_to_register[frame_in_flight_idx].clear();
}