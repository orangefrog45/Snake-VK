#pragma once
#include "assets/MeshData.h"
#include "TextureAssets.h"
#include "MaterialAsset.h"

namespace SNAKE {
	class AssetLoader {
	public:
		// Returns formatted filename (not path, just filename) for p_asset for serialization
		// file_extension shouldn't include a "."
		inline static std::string GenAssetFilename(Asset* p_asset, const std::string& file_extension) {
			return std::format("{}_{}.{}", p_asset->name, p_asset->uuid(), file_extension);
		}

		// Serializes mesh data asset along with all raw mesh data (vertex data etc) required to load it
		static void SerializeMeshDataBinary(const std::string& output_filepath, const MeshData& data, MeshDataAsset& asset);

		// Serializes material to binary file, may switch to json later given the small size
		static void SerializeMaterialBinary(const std::string& output_filepath, MaterialAsset& asset);

		// Creates a json file at output_filepath containing asset data (source data asset etc)
		static void SerializeStaticMeshAsset(const std::string& output_filepath, StaticMeshAsset& asset);

		// Reads json file at input_filepath and deserializes contents into asset
		static StaticMeshAsset* DeserializeStaticMeshAsset(const std::string& input_filepath);

		// Serializes texture asset and image data to output_filepath
		// filepath_raw should be a path to the image file (.jpg, .png, etc) the texture was loaded from
		static void SerializeTexture2DBinaryFromRawFile(const std::string& output_filepath, Texture2DAsset& asset, 
			const std::string& filepath_raw);

		// Serializes texture asset data to output_filepath
		// Will keep image data the same in the asset file, will only overwrite texture parameters (format etc)
		static void SerializeTexture2DBinary(const std::string& output_filepath, Texture2DAsset& asset);

		static Texture2DAsset* DeserializeTexture2D(const std::string& filepath);

		static MaterialAsset* DeserializeMaterial(const std::string& filepath);

		static MeshDataAsset* DeserializeMeshData(const std::string& filepath);

		// Loads mesh_data_asset's gpu buffers, materials etc from "data"
		//static void LoadMeshFromData(AssetRef<MeshDataAsset> mesh_data_asset, MeshData& data);
		
		// Creates and loads a MeshData struct from a raw 3d model file supported by assimp (.obj, .fbx etc)
		static std::unique_ptr<MeshData> LoadMeshDataFromRawFile(const std::string& filepath, bool load_materials_and_textures = true);

		// Loads texture data from raw image file supported by stb_image (.png, .jpg etc)
		static bool LoadTextureFromFile(AssetRef<Texture2DAsset> tex, vk::Format fmt);

	private:
		// Returns [TextureRef, is_texture_newly_created]
		static std::pair<AssetRef<Texture2DAsset>, bool> CreateOrGetTextureFromMaterial(const std::string& dir, aiTextureType type, aiMaterial* p_material);
	};
}