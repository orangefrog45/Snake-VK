#pragma once
#include "Asset.h"
#include "util/util.h"
#include "core/DescriptorBuffer.h"
#include "assets/ResourceBufferManagers.h"
#include "assets/MeshData.h"

namespace SNAKE {
	enum class AssetEventType {
		CREATED,
		DESTROYED
	};

	struct AssetEvent : public Event {
		AssetEvent(AssetEventType _type, Asset* _asset) : type(_type), p_asset(_asset) {}

		AssetEventType type;
		Asset* p_asset;
	};

	class AssetManager {
	public:
		friend class AssetLoader;

		inline static AssetManager& Get() {
			static AssetManager instance;
			return instance;
		}

		static void Init() { Get().I_Init(); }
		static void Shutdown();

		static void DeleteAsset(Asset* asset);
		static void DeleteAsset(uint64_t uuid);

		template<std::derived_from<Asset> AssetT>
		void OnAssetAdd(AssetRef<AssetT> asset) {
			if constexpr (std::is_same_v<AssetT, MaterialAsset>) {
				m_global_material_buffer_manager.RegisterMaterial(asset);
			}

			EventManagerG::DispatchEvent(AssetEvent{ AssetEventType::CREATED, asset.get()});
		}

		void OnAssetDelete(Asset* p_asset) {
			EventManagerG::DispatchEvent(AssetEvent{ AssetEventType::DESTROYED, p_asset });
		}

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

			Get().OnAssetAdd(AssetRef(p_asset));

			return AssetRef<AssetT>(*p_asset);
		}

		enum CoreAssetIDs {
			SPHERE_MESH = 1,
			SPHERE_MESH_DATA,
			MATERIAL,
			TEXTURE,
		};

		template<std::derived_from<Asset> T>
		static T* GetAssetRaw(uint64_t uuid) {
			if (!Get().m_assets.contains(uuid)) {
				SNK_CORE_ERROR("AssetManager::GetAsset failed, UUID '{}' doesn't exist in AssetManager", uuid);
				return nullptr;
			}

			auto* p_asset = dynamic_cast<T*>(Get().m_assets[uuid]);
			if (p_asset == nullptr) {
				SNK_CORE_ERROR("AssetManager::GetAsset failed '{}', wrong asset type", uuid);
			}

			return p_asset;
		}

		template<std::derived_from<Asset> T>
		static AssetRef<T> GetAsset(uint64_t uuid) {
			return AssetRef<T>{	GetAssetRaw<T>(uuid) };
		}

		template<std::derived_from<Asset> T>
		static AssetRef<T> GetAsset(const std::string& filepath) {
			for (auto& [uuid, p_asset] : Get().m_assets) {
				if (std::filesystem::equivalent(p_asset->filepath, filepath)) {
					if (auto* p_casted = dynamic_cast<T*>(p_asset)) {
						return AssetRef<T>(p_casted);
					}
					else {
						SNK_CORE_ERROR("AssetManager::GetAsset failed '{}', wrong asset type", p_asset->filepath);
					}
				}
			}

			SNK_CORE_ERROR("AssetManager::GetAsset failed, filepath '{}' doesn't exist in AssetManager", filepath);
			return nullptr;
		}

		template<std::derived_from<Asset> T>
		static std::vector<T*> GetView() {
			std::vector<T*> ret;
			for (auto& [uuid, p_asset] : Get().m_assets) {
				if (IsCoreAssetUUID(uuid))
					continue;

				if (auto* p_casted = dynamic_cast<T*>(p_asset))
					ret.push_back(p_casted);
			}
			return ret;
		}

		static void Clear() {
			std::vector<uint64_t> deletion_queue;
			auto& assets = Get().m_assets;
			for (auto [uuid, p_asset] : assets) {
				if (IsCoreAssetUUID(uuid))
					continue;
				else
					deletion_queue.push_back(uuid);
			}

			for (auto uuid : deletion_queue) {
				DeleteAsset(uuid);
			}
		}

		static bool IsCoreAssetUUID(uint64_t uuid) {
			return uuid != 0 && uuid <= NUM_CORE_ASSETS;
		}

		static vk::DeviceAddress GetGlobalTexMatBufDeviceAddress(uint32_t current_frame) {
			return Get().m_global_tex_buffer_manager.descriptor_buffers[current_frame]->descriptor_buffer.GetDeviceAddress();
		}

		static vk::DescriptorBufferBindingInfoEXT GetGlobalTexMatBufBindingInfo(uint32_t current_frame) {
			return Get().m_global_material_buffer_manager.descriptor_buffers[current_frame]->GetBindingInfo();
		}


	private:
		static constexpr size_t NUM_CORE_ASSETS = 4;
		void LoadCoreAssets();
		void I_Init();

		void InitGlobalBufferManagers();

		std::unordered_map<uint64_t, Asset*> m_assets;

		GlobalTextureBufferManager m_global_tex_buffer_manager;
		GlobalMaterialBufferManager m_global_material_buffer_manager;
	};
}