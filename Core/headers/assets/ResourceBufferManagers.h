#pragma once
#include "core/VkCommon.h"
#include "core/VkContext.h"
#include "textures/Textures.h"
#include "assets/MaterialAsset.h"
#include "core/DescriptorBuffer.h"

namespace SNAKE {

	class GlobalMaterialBufferManager {
	public:
		void Init(const std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT>& buffers);

		void RegisterMaterial(AssetRef<MaterialAsset> material);

		// Size is doubled due to alignment requiring it, otherwise buffer sizes don't match on allocation
		// Will probably use up the space for something in the future anyway
		inline static constexpr uint32_t material_size = sizeof(uint32_t) * 8 * 2;

		std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT> descriptor_buffers{};
	private:
		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> material_ubos;

		void UpdateMaterialUBO();

		EventListener m_material_update_event_listener;
		EventListener m_frame_start_listener;
		// Current index to write to 
		uint16_t m_current_index = 0;

		std::array<S_VkBuffer, MAX_FRAMES_IN_FLIGHT> m_material_ubos;

		std::unordered_map<FrameInFlightIndex, std::vector<AssetRef<MaterialAsset>>> m_materials_to_register;

		std::unordered_map<FrameInFlightIndex, std::vector<AssetRef<MaterialAsset>>> m_materials_to_update;
	};

	class GlobalTextureBufferManager {
	public:
		void Init(const std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT>& buffers);

		void RegisterTexture(Texture2D& tex);

		std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT> descriptor_buffers{};
	private:
		void RegisterTexturesInternal();

		uint16_t m_current_index = 0;

		EventListener m_frame_start_listener;

		std::unordered_map<FrameInFlightIndex, std::vector<Texture2D*>> m_textures_to_register;
	};
}