#pragma once
#include "assets/MeshData.h"
#include "TextureAssets.h"
#include "MaterialAsset.h"
#include "AssetManager.h"

namespace SNAKE {
	struct AssetSerializer {
		static void SerializeMeshDataBinary(const std::string& output_filepath, const AssetManager::MeshData& data, const MeshDataAsset& asset);

		static void SerializeMaterialBinary(const std::string& output_filepath, MaterialAsset& asset);

		// Serializes texture asset and image data to output_filepath
		// filepath_raw should be a path to the image file (.jpg, .png, etc) the texture was loaded from
		static void SerializeTexture2DBinaryFromRawFile(const std::string& output_filepath, Texture2DAsset& asset, 
			const std::string& filepath_raw);

		static bool DeserializeTexture2D(const std::string filepath, Texture2DAsset& asset);

		static bool DeserializeMaterial(const std::string filepath, MaterialAsset& asset);

		static bool DeserializeMeshData(const std::string filepath, MeshDataAsset& asset);




		// Serializes texture asset data to output_filepath
		// Will keep image data the same in the asset file, will only overwrite texture parameters (format etc)
		//static void SerializeTexture2DBinary(const std::string& output_filepath, Texture2DAsset& asset);

	};
}