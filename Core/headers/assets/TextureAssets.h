#pragma once
#include "images/Images.h"
#include "Asset.h"

namespace SNAKE {
	class Texture2DAsset : public Asset {
	public:
		Texture2DAsset(uint64_t uuid = 0) : Asset(uuid) {};

		inline uint16_t GetGlobalIndex() const { return m_global_index; };

		void LoadFromFile(const std::string& filepath);

		inline static constexpr uint16_t INVALID_GLOBAL_INDEX = std::numeric_limits<uint16_t>::max();

		Image2D image;
	private:
		// Index into the global texture descriptor buffer
		uint16_t m_global_index = INVALID_GLOBAL_INDEX;

		friend class GlobalTextureBufferManager;
		friend class AssetManager;
	};
}