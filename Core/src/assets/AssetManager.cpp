#include "pch/pch.h"
#include "assets/AssetManager.h"
#include "util/util.h"

namespace SNAKE {

	void AssetManager::I_Init() {
		InitGlobalBufferManagers();
		LoadCoreAssets();
	}

	void AssetManager::InitGlobalBufferManagers() {
		std::array<std::shared_ptr<DescriptorBuffer>, MAX_FRAMES_IN_FLIGHT> descriptor_buffers;

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			descriptor_buffers[i] = std::make_shared<DescriptorBuffer>();

			auto p_descriptor_spec = std::make_shared<DescriptorSetSpec>();
			descriptor_buffers[i]->SetDescriptorSpec(p_descriptor_spec);

			p_descriptor_spec->AddDescriptor(0, vk::DescriptorType::eUniformBuffer,
				vk::ShaderStageFlagBits::eAllGraphics, 64)
				.AddDescriptor(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eAllGraphics, 4096);

			p_descriptor_spec->GenDescriptorLayout();

			descriptor_buffers[i]->CreateBuffer(1);
		}

		m_global_material_buffer_manager.Init(descriptor_buffers);
		m_global_tex_buffer_manager.Init(descriptor_buffers);
	}

	void AssetManager::LoadCoreAssets() {

	}

	void AssetManager::Shutdown() {
		auto& assets = Get().m_assets;

		while (!assets.empty()) {
			DeleteAsset(assets.begin()->second);
		}
	}

	void AssetManager::DeleteAsset(Asset* asset) {
		if (!Get().m_assets.contains(asset->uuid())) {
			SNK_CORE_ERROR("AssetManager::DeleteAsset failed '[{}, {}]', UUID doesn't exist in AssetManager", asset->uuid(), asset->filepath);
			return;
		}

		if (auto ref_count = asset->GetRefCount(); ref_count != 0) {
			SNK_CORE_WARN("Deleting asset '[{}, {}]' which still has {} references", asset->uuid(), asset->filepath, ref_count);
		}

		delete Get().m_assets[asset->uuid()];
	}

	void AssetManager::DeleteAsset(uint64_t uuid) {
		auto& assets = Get().m_assets;

		if (!assets.contains(uuid)) {
			SNK_CORE_ERROR("AssetManager::DeleteAsset failed, UUID '{}' doesn't exist in AssetManager", uuid);
			return;
		}

		auto* p_asset = assets[uuid];

		if (auto ref_count = p_asset->GetRefCount(); ref_count != 0) {
			SNK_CORE_WARN("Deleting asset '[{}, {}]' which still has {} references", p_asset->uuid(), p_asset->filepath, ref_count);
		}

		delete assets[uuid];
	}

	
}