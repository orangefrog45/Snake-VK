#include "pch/pch.h"
#include "assets/TextureAssets.h"
#include "assets/AssetManager.h"

using namespace SNAKE;

void Texture2DAsset::LoadFromFile(const std::string& filepath) {
	AssetManager::LoadTextureFromFile(AssetRef(this), filepath);
}