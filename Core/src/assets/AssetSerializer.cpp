#include "pch/pch.h"
#include "assets/AssetSerializer.h"
#include "stb_image.h"
#include "util/ByteSerializer.h"

using namespace SNAKE;

void AssetSerializer::SerializeTexture2DBinaryFromRawFile(const std::string& output_filepath, Texture2DAsset& asset, 
	const std::string& filepath_raw) {
	asset.filepath = output_filepath;

	ByteSerializer ser;
	auto& spec = asset.image.GetSpec();
	ser.Value(asset.uuid());
	ser.Value(spec.format);
	ser.Value(spec.mip_levels);
	ser.Value(spec.size);

	std::vector<std::byte> raw_file_data; 
	files::ReadBinaryFile(filepath_raw, raw_file_data);

	files::WriteBinaryFile("TEST.jpg", raw_file_data.data(), raw_file_data.size());
	ser.Container(raw_file_data);
	SNK_CORE_INFO(raw_file_data.size());
	int x, y, channels;
	auto pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(raw_file_data.data()), raw_file_data.size(), &x, &y, &channels, 0);
	SNK_ASSERT(pixels);
	ser.OutputToFile(output_filepath);
}

bool AssetSerializer::DeserializeTexture2D(const std::string filepath, Texture2DAsset& asset) {
	SNK_ASSERT(filepath.ends_with(".tex2d"));
	std::vector<std::byte> data;
	files::ReadBinaryFile(filepath, data);

	if (data.empty()) {
		SNK_CORE_ERROR("DeserializeTexture2D failed, binary data failed to load from file '{}'", filepath);
		return false;
	}

	Image2DSpec spec;
	spec.aspect_flags = vk::ImageAspectFlagBits::eColor;
	spec.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;
	spec.tiling = vk::ImageTiling::eOptimal;

	ByteDeserializer d{ data.data(), data.size() };
	uint64_t uuid;
	d.Value(uuid);
	asset.uuid = UUID<uint64_t>(uuid);
	d.Value(spec.format);
	d.Value(spec.mip_levels);
	d.Value(spec.size);

	std::vector<std::byte> raw_file_data;
	d.Container(raw_file_data);
	SNK_CORE_INFO(raw_file_data.size());

	int x, y, channels;
	auto pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(raw_file_data.data()), raw_file_data.size(), &x, &y, &channels, 4);
	if (!pixels) 
		SNK_CORE_ERROR("DeserializeTexture2D failed, stbi load failed '{}'", stbi_failure_reason());

	size_t image_size = x * y * 4;

	S_VkBuffer staging_buffer{};
	staging_buffer.CreateBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
	void* p_data = staging_buffer.Map();
	memcpy(p_data, pixels, (size_t)image_size);
	staging_buffer.Unmap();
	delete[] pixels;

	asset.image.SetSpec(spec);
	asset.image.CreateImage();

	asset.image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 0, spec.mip_levels);
	CopyBufferToImage(staging_buffer.buffer, asset.image.GetImage(), spec.size.x, spec.size.y);
	asset.image.GenerateMipmaps(vk::ImageLayout::eTransferDstOptimal);
	asset.image.TransitionImageLayout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 0, spec.mip_levels);

	asset.image.CreateImageView();
	asset.image.CreateSampler();

	AssetManager::Get().m_global_tex_buffer_manager.RegisterTexture(&asset);
}

void AssetSerializer::SerializeMaterialBinary(const std::string& output_filepath, MaterialAsset& asset) {
	ByteSerializer ser;
	ser.Value(asset.uuid());
	ser.Value(asset.albedo);
	ser.Value(asset.emissive);
	ser.Value(asset.roughness);
	ser.Value(asset.metallic);
	ser.Value(asset.ao);
	ser.Value(asset.albedo_tex ? asset.albedo_tex->uuid() : UUID<uint64_t>::INVALID_UUID);
	ser.Value(asset.normal_tex ? asset.normal_tex->uuid() : UUID<uint64_t>::INVALID_UUID);
	ser.Value(asset.roughness_tex ? asset.roughness_tex->uuid() : UUID<uint64_t>::INVALID_UUID);
	ser.Value(asset.metallic_tex ? asset.metallic_tex->uuid() : UUID<uint64_t>::INVALID_UUID);
	ser.Value(asset.ao_tex ? asset.ao_tex->uuid() : UUID<uint64_t>::INVALID_UUID);
	ser.OutputToFile(output_filepath);
}

void AssetSerializer::SerializeMeshDataBinary(const std::string& output_filepath, const AssetManager::MeshData& data, const MeshDataAsset& asset) {
	auto* p_scene = data.importer.GetScene();
	ByteSerializer s;
	s.Value(asset.uuid());
	s.Container(asset.submeshes);
	unsigned total_vertices = 0;
	for (size_t i = 0; i < asset.submeshes.size(); i++) {
		total_vertices += p_scene->mMeshes[i]->mNumVertices;
	}
	s.Value(total_vertices);

	unsigned vertex_offset = 0;

	for (size_t i = 0; i < asset.submeshes.size(); i++) {
		auto& submesh = *p_scene->mMeshes[i];
		s.Value(reinterpret_cast<std::byte*>(submesh.mVertices) + vertex_offset * sizeof(aiVector3D), submesh.mNumVertices * sizeof(aiVector3D));
		vertex_offset += submesh.mNumVertices;
	}

	vertex_offset = 0;

	for (size_t i = 0; i < asset.submeshes.size(); i++) {
		auto& submesh = *p_scene->mMeshes[i];
		s.Value(reinterpret_cast<std::byte*>(submesh.mNormals) + vertex_offset * sizeof(aiVector3D), submesh.mNumVertices * sizeof(aiVector3D));
		vertex_offset += submesh.mNumVertices;
	}

	vertex_offset = 0;

	for (size_t i = 0; i < asset.submeshes.size(); i++) {
		auto& submesh = *p_scene->mMeshes[i];
		s.Value(reinterpret_cast<std::byte*>(submesh.mTangents) + vertex_offset * sizeof(aiVector3D), submesh.mNumVertices * sizeof(aiVector3D));
		vertex_offset += submesh.mNumVertices;
	}

	s.Container(data.indices);
	s.Container(data.tex_coords);

	s.Value(asset.materials.size());
	for (auto mat : asset.materials) {
		s.Value(mat->uuid());
	}

	s.OutputToFile(output_filepath);
}




bool AssetSerializer::DeserializeMaterial(const std::string filepath, MaterialAsset& asset) {
	bool ret = false;

	return ret;
}

bool AssetSerializer::DeserializeMeshData(const std::string filepath, MeshDataAsset& asset) {
	bool ret = false;

	return ret;
}

