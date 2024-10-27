#pragma once
#include "assets/TextureAssets.h"

namespace SNAKE {
	class MaterialAsset : public Asset {
	public:
		struct MaterialUpdateEvent : public Event {
			MaterialUpdateEvent(MaterialAsset* _mat) : p_material(_mat) { };
			MaterialAsset* p_material = nullptr;
		};

		enum class MaterialFlagBits : uint32_t {
			EMISSIVE = 1 << 0,
		};

		using MaterialFlags = uint32_t;
		MaterialFlags flags = 0;

		uint32_t GetGlobalBufferIndex() {
			return m_global_buffer_index;
		}

		// Call after modifying any material data for this material to sequence updates for the gpu buffers
		void DispatchUpdateEvent() {
			EventManagerG::DispatchEvent(MaterialUpdateEvent(this));
		}

		void RaiseFlag(MaterialFlagBits flag) {
			flags = flags | (uint32_t)flag;
		}

		void RemoveFlag(MaterialFlagBits flag) {
			flags = flags & ~((uint32_t)flag);
		}

		void FlipFlag(MaterialFlagBits flag) {
			if (flags & (uint32_t)flag)
				RemoveFlag(flag);
			else
				RaiseFlag(flag);
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