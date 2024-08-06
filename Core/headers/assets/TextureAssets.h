#pragma once
#include "textures/Textures.h"
#include "Asset.h"

namespace SNAKE {
	class Texture2DAsset : public Asset {
	public:
		std::unique_ptr<Texture2D> p_tex = nullptr;
	private:
		Texture2DAsset(uint64_t uuid = 0) : Asset(uuid) {};
		friend class AssetManager;
	};
}