#include "pch/pch.h"
#include "assets/TextureAssets.h"
#include "assets/AssetManager.h"

using namespace SNAKE;

void Texture2DAsset::LoadFromFile(const std::string& _filepath) {
	filepath = _filepath;
	AssetManager::LoadTextureFromFile(AssetRef(this));
}