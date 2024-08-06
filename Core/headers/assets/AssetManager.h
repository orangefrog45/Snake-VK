#pragma once
#include "Asset.h"
#include "util/util.h"

namespace SNAKE {
	class AssetManager {
	public:
		inline static AssetManager& Get() {
			static AssetManager instance;
			return instance;
		}

		static void Init() { Get().LoadCoreAssets(); }
		static void Shutdown();

		static void DeleteAsset(Asset* asset);
		static void DeleteAsset(uint64_t uuid);

		template<std::derived_from<Asset> AssetT, typename... Args>
		static AssetRef<AssetT> CreateAsset(uint64_t uuid = 0, Args&&... args) {
			if (Get().m_assets.contains(uuid)) {
				SNK_CORE_ERROR("AssetManager::CreateAsset failed, UUID conflict: '{}'", uuid);
				return GetAsset<AssetT>(uuid);
			}

			if (uuid == 0)
				uuid = UUID<uint64_t>()();

			auto* p_asset = new AssetT(uuid, std::forward<Args>(args)...);
			Get().m_assets[uuid] = p_asset;

			return AssetRef<AssetT>(*p_asset);
		}

		template<typename T>
		static AssetRef<T> GetAsset(uint64_t uuid) {
			if (!Get().m_assets.contains(uuid)) {
				SNK_CORE_ERROR("AssetManager::GetAsset failed, UUID '{}' doesn't exist in AssetManager", uuid);
				return AssetRef<T>{ nullptr };
			}

			auto* p_asset = dynamic_cast<T*>(Get().m_assets[uuid]);
			if (p_asset == nullptr) {
				SNK_CORE_ERROR("AssetManager::GetAsset failed '{}', wrong asset type", uuid);
			}

			return AssetRef<T>{	p_asset };
		}

		inline static constexpr uint32_t NUM_CORE_ASSETS = 1;
	private:
		void LoadCoreAssets();

		std::unordered_map<uint64_t, Asset*> m_assets;
	};
}