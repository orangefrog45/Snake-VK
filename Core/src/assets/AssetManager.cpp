#include "pch/pch.h"
#include "assets/AssetManager.h"
#include "util/util.h"

namespace SNAKE {

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