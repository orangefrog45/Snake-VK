#include "pch/pch.h"
#include "assets/ResourceBufferManagers.h"

using namespace SNAKE;

void GlobalMaterialBufferManager::Init(const std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT>& buffers) {
	descriptor_buffers = buffers;
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_material_ubos[i].CreateBuffer(4096 * material_size,
			vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);

		// Get address + size of uniform buffer
		vk::DescriptorAddressInfoEXT addr_info{};
		addr_info.address = m_material_ubos[i].GetDeviceAddress();
		addr_info.range = m_material_ubos[i].alloc_info.size;

		// Use above address + size data to connect the descriptor at the offset provided to this specific UBO
		vk::DescriptorGetInfoEXT buffer_descriptor_info{};
		buffer_descriptor_info.type = vk::DescriptorType::eUniformBuffer;
		buffer_descriptor_info.data.pUniformBuffer = &addr_info;

		VulkanContext::GetLogicalDevice().device->getDescriptorEXT(
			buffer_descriptor_info,
			VulkanContext::GetPhysicalDevice().buffer_properties.uniformBufferDescriptorSize,
			reinterpret_cast<std::byte*>(descriptor_buffers[i]->descriptor_buffer.Map()) +
			descriptor_buffers[i]->GetDescriptorSpec()->GetBindingOffset(0));
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

	EventManagerG::RegisterListener<MaterialAsset::MaterialUpdateEvent>(m_material_update_event_listener);
	EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_listener);
}

void GlobalMaterialBufferManager::RegisterMaterial(AssetRef<MaterialAsset> material) {
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_materials_to_register[i].push_back(material);
		m_materials_to_update[i].push_back(material);
	}
	material->m_global_buffer_index = m_current_index++;
}

void GlobalMaterialBufferManager::UpdateMaterialUBO() {
	auto frame_in_flight_idx = VulkanContext::GetCurrentFIF();

	std::byte* p_data = reinterpret_cast<std::byte*>(m_material_ubos[frame_in_flight_idx].Map());

	for (auto& mat_ref : m_materials_to_update[frame_in_flight_idx]) {
		SNK_ASSERT(mat_ref->GetGlobalBufferIndex() != MaterialAsset::INVALID_GLOBAL_INDEX);

		std::array<uint32_t, 5> texture_indices = { Texture2D::INVALID_GLOBAL_INDEX };

		if (mat_ref->p_albedo_tex) texture_indices[0] = mat_ref->p_albedo_tex->GetGlobalIndex();
		if (mat_ref->p_normal_tex) texture_indices[1] = mat_ref->p_normal_tex->GetGlobalIndex();
		if (mat_ref->p_roughness_tex) texture_indices[2] = mat_ref->p_roughness_tex->GetGlobalIndex();
		if (mat_ref->p_metallic_tex) texture_indices[3] = mat_ref->p_metallic_tex->GetGlobalIndex();
		if (mat_ref->p_ao_tex) texture_indices[4] = mat_ref->p_ao_tex->GetGlobalIndex();

		std::array<float, 3> params = { mat_ref->roughness, mat_ref->metallic, mat_ref->ao };

		memcpy(p_data + mat_ref->GetGlobalBufferIndex() * material_size, texture_indices.data(), texture_indices.size() * sizeof(uint32_t));
		memcpy(p_data + mat_ref->GetGlobalBufferIndex() * material_size + texture_indices.size() * sizeof(uint32_t), params.data(), params.size() * sizeof(float));
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

void GlobalTextureBufferManager::RegisterTexture(Texture2D& tex) {
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_textures_to_register[i].push_back(&tex);
	}
	tex.m_global_index = m_current_index++;
}

void GlobalTextureBufferManager::RegisterTexturesInternal() {
	auto frame_in_flight_idx = VulkanContext::GetCurrentFIF();
	for (auto* p_tex : m_textures_to_register[frame_in_flight_idx]) {
		auto& buffer_properties = VulkanContext::GetPhysicalDevice().buffer_properties;

		vk::DescriptorImageInfo image_descriptor{};
		image_descriptor.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		image_descriptor.imageView = p_tex->GetImageView();
		image_descriptor.sampler = p_tex->GetSampler();

		// Use above image data to connect the descriptor at the offset provided to this specific image
		vk::DescriptorGetInfoEXT image_desc_info{};
		image_desc_info.type = vk::DescriptorType::eCombinedImageSampler;
		image_desc_info.data.pCombinedImageSampler = &image_descriptor;
		VulkanContext::GetLogicalDevice().device->getDescriptorEXT(image_desc_info,
			buffer_properties.combinedImageSamplerDescriptorSize,
			reinterpret_cast<std::byte*>(descriptor_buffers[frame_in_flight_idx]->descriptor_buffer.Map()) + p_tex->GetGlobalIndex() *
			buffer_properties.combinedImageSamplerDescriptorSize + descriptor_buffers[frame_in_flight_idx]->GetDescriptorSpec()->GetBindingOffset(1));

	}

	m_textures_to_register[frame_in_flight_idx].clear();
}