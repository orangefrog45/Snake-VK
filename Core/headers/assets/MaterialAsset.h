#pragma once
#include "Asset.h"
#include "textures/Textures.h"

namespace SNAKE {
	class MaterialAsset : public Asset {
	public:
		struct MaterialUpdateEvent : public Event {
			MaterialUpdateEvent(MaterialAsset* _mat) : p_material(_mat) { };
			MaterialAsset* p_material = nullptr;
		};

		uint16_t GetGlobalBufferIndex() {
			return m_global_buffer_index;
		}

		// Call after modifying any material data for this material to sequence updates for the gpu buffers
		void DispatchUpdateEvent() {
			EventManagerG::DispatchEvent(MaterialUpdateEvent(this));
		}

		Texture2D* p_albedo_tex = nullptr;
		Texture2D* p_normal_tex = nullptr;
		Texture2D* p_roughness_tex = nullptr;
		Texture2D* p_metallic_tex = nullptr;
		Texture2D* p_ao_tex = nullptr;

		float roughness = 0.5f;
		float metallic = 0.f;
		float ao = 0.2f;
	private:
		MaterialAsset(uint64_t uuid = 0) : Asset(uuid) {};

		inline static constexpr uint16_t INVALID_GLOBAL_INDEX = std::numeric_limits<uint16_t>::max();

		uint16_t m_global_buffer_index = INVALID_GLOBAL_INDEX;

		friend class GlobalMaterialBufferManager;
		friend class AssetManager;
	};

}