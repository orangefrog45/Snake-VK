#pragma once
#include "resources/Images.h"
#include "Asset.h"

namespace SNAKE {
	class Texture2DAsset : public Asset {
	public:
		Texture2DAsset(uint64_t uuid = 0) : Asset(uuid) {};

		inline uint32_t GetGlobalIndex() const { return m_global_index; };

		inline static constexpr uint32_t INVALID_GLOBAL_INDEX = std::numeric_limits<uint32_t>::max();

		Image2D image;
	private:
		// Index into the global texture descriptor buffer
		uint32_t m_global_index = INVALID_GLOBAL_INDEX;

		friend class GlobalTextureBufferManager;
		friend class AssetManager;
	};
}