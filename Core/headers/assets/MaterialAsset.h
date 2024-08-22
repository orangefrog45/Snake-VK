#pragma once
#include "assets/TextureAssets.h"

namespace SNAKE {
	class MaterialAsset : public Asset {
	public:
		struct MaterialUpdateEvent : public Event {
			MaterialUpdateEvent(MaterialAsset* _mat) : p_material(_mat) { };
			MaterialAsset* p_material = nullptr;
		};

		enum MaterialFlags {
	
		};

		uint32_t GetGlobalBufferIndex() {
			return m_global_buffer_index;
		}

		// Call after modifying any material data for this material to sequence updates for the gpu buffers
		void DispatchUpdateEvent() {
			EventManagerG::DispatchEvent(MaterialUpdateEvent(this));
		}

		AssetRef<Texture2DAsset> albedo_tex = nullptr;
		AssetRef<Texture2DAsset> normal_tex = nullptr;
		AssetRef<Texture2DAsset> roughness_tex = nullptr;
		AssetRef<Texture2DAsset> metallic_tex = nullptr;
		AssetRef<Texture2DAsset> ao_tex = nullptr;

		glm::vec3 albedo{ 1, 1, 1 };
		float emissive = 0.f;
		float roughness = 0.5f;
		float metallic = 0.f;
		float ao = 0.2f;
	private:
		MaterialAsset(uint64_t uuid = 0) : Asset(uuid) {};

		inline static constexpr uint32_t INVALID_GLOBAL_INDEX = std::numeric_limits<uint32_t>::max();

		uint32_t m_global_buffer_index = INVALID_GLOBAL_INDEX;

		friend class GlobalMaterialBufferManager;
		friend class AssetManager;
	};

}