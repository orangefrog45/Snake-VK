#include "pch/pch.h"
#include "assets/TextureAssets.h"
#include "assets/AssetManager.h"

using namespace SNAKE;

void Texture2DAsset::LoadFromFile(const std::string& _filepath, vk::Format fmt) {
	filepath = _filepath;
	AssetManager::LoadTextureFromFile(AssetRef(this), fmt);
}