#include "pch/pch.h"
#include "assets/AssetLoader.h"
#include "assets/AssetManager.h"
#include "stb_image.h"
#include "util/ByteSerializer.h"
#include "assets/MeshData.h"
#include "nlohmann/json.hpp"

using namespace SNAKE;

void AssetLoader::SerializeTexture2DBinaryFromRawFile(const std::string& output_filepath, Texture2DAsset& asset, 
	const std::string& filepath_raw) {
	SNK_ASSERT(output_filepath.ends_with(".tex2d"));
	asset.filepath = output_filepath;

	ByteSerializer ser;
	auto& spec = asset.image.GetSpec();
	ser.Value(asset.uuid());
	ser.Container(asset.name);
	ser.Value(spec.format);
	ser.Value(spec.mip_levels);
	ser.Value(spec.size);

	std::vector<std::byte> raw_file_data; 
	if (!files::ReadBinaryFile(filepath_raw, raw_file_data)) {
		SNK_CORE_ERROR("SerializeTexture2DBinaryFromRawFile failed, couldn't read raw file '{}'", filepath_raw);
		return;
	}
	ser.Container(raw_file_data);
	ser.OutputToFile(output_filepath);
}

void AssetLoader::SerializeTexture2DBinary(const std::string& output_filepath, Texture2DAsset& asset) {
	SNK_ASSERT_ARG(files::PathExists(output_filepath), "Output filepath must already exist (texture must already have been serialized), call SerializeTexture2DBinaryFromRawFile first");
	
	std::vector<std::byte> raw_image_data;

	// First fetch and read the existing raw image data
	{
		std::vector<std::byte> data;
		if (!files::ReadBinaryFile(output_filepath, data)) {
			SNK_CORE_ERROR("SerializeTexture2DBinary failed, couldn't read binary file '{}'", output_filepath);
			return;
		}
		ByteDeserializer d{ data.data(), data.size() };

		std::string throwaway_name;
		size_t throwaway_uuid;
		Image2DSpec throwaway_spec;
		d.Value(throwaway_uuid);
		d.Container(throwaway_name);
		d.Value(throwaway_spec.format);
		d.Value(throwaway_spec.mip_levels);
		d.Value(throwaway_spec.size);
		d.Container(raw_image_data);
	}

	ByteSerializer ser;
	auto& spec = asset.image.GetSpec();
	ser.Value(asset.uuid());
	ser.Container(asset.name);
	ser.Value(spec.format);
	ser.Value(spec.mip_levels);
	ser.Value(spec.size);
	files::WriteBinaryFile("TEST.jpg", raw_image_data.data(), raw_image_data.size());
	ser.Container(raw_image_data);
	ser.OutputToFile(output_filepath);
}


Texture2DAsset* AssetLoader::DeserializeTexture2D(const std::string& filepath) {
	SNK_ASSERT(filepath.ends_with(".tex2d"));
	std::vector<std::byte> data;
	files::ReadBinaryFile(filepath, data);

	if (data.empty()) {
		SNK_CORE_ERROR("DeserializeTexture2D failed, binary data failed to load from file '{}'", filepath);
		return nullptr;
	}

	Image2DSpec spec;
	spec.aspect_flags = vk::ImageAspectFlagBits::eColor;
	spec.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;
	spec.tiling = vk::ImageTiling::eOptimal;

	ByteDeserializer d{ data.data(), data.size() };
	uint64_t uuid;
	d.Value(uuid);
	auto& asset = *AssetManager::CreateAsset<Texture2DAsset>(uuid).get();
	asset.filepath = filepath;
	d.Container(asset.name);
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
	return &asset;
}

void AssetLoader::SerializeMaterialBinary(const std::string& output_filepath, MaterialAsset& asset) {
	SNK_ASSERT(output_filepath.ends_with(".mat"));
	asset.filepath = output_filepath;
	ByteSerializer ser;
	ser.Value(asset.uuid());
	ser.Container(asset.name);
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

void AssetLoader::SerializeMeshDataBinary(const std::string& output_filepath, const MeshData& data, MeshDataAsset& asset) {
	SNK_ASSERT(output_filepath.ends_with(".meshdata"));
	asset.filepath = output_filepath;
	ByteSerializer s;
	s.Value(asset.uuid());
	s.Container(asset.name);
	s.Container(data.submeshes);
	s.Value(data.num_vertices);
	s.Value(data.num_indices);

	s.Value(reinterpret_cast<const std::byte*>(data.positions), data.num_vertices * sizeof(aiVector3D));
	s.Value(reinterpret_cast<const std::byte*>(data.normals), data.num_vertices * sizeof(aiVector3D));
	s.Value(reinterpret_cast<const std::byte*>(data.tangents), data.num_vertices * sizeof(aiVector3D));
	s.Value(reinterpret_cast<const std::byte*>(data.tex_coords), data.num_vertices * sizeof(aiVector2D));
	s.Value(reinterpret_cast<const std::byte*>(data.indices), data.num_indices * sizeof(unsigned));

	s.Container(data.materials);
	s.Container(data.textures);
	s.OutputToFile(output_filepath);
}


MeshDataAsset* AssetLoader::DeserializeMeshData(const std::string& filepath) {
	SNK_ASSERT(filepath.ends_with(".meshdata"));
	std::vector<std::byte> data;
	files::ReadBinaryFile(filepath, data);
	if (data.empty()) {
		SNK_CORE_ERROR("DeserializeMeshData failed, reading filepath '{}' gave no data", filepath);
		return nullptr;
	}
	ByteDeserializer d{ data.data(), data.size() };

	MeshData mesh_data;
	uint64_t uuid;
	d.Value(uuid);
	auto& asset = *AssetManager::CreateAsset<MeshDataAsset>(uuid).get();
	asset.uuid = UUID(uuid);
	asset.filepath = filepath;

	d.Container(asset.name);
	d.Container(mesh_data.submeshes);
	d.Value(mesh_data.num_vertices);
	d.Value(mesh_data.num_indices);

	d.Value(mesh_data.positions, 0);
	d.Value(mesh_data.normals, 0);
	d.Value(mesh_data.tangents, 0);
	d.Value(mesh_data.tex_coords, 0);
	d.Value(mesh_data.indices, 0);

	d.Container(mesh_data.materials);
	d.Container(mesh_data.textures);

	for (size_t i = 0; i < mesh_data.materials.size(); i++) {
		auto mat = AssetManager::GetAsset<MaterialAsset>(mesh_data.materials[i]);
		if (!mat) {
			SNK_CORE_ERROR("Tried loading material '{}' in DeserializeMeshData, UUID not found", mesh_data.materials[i]);
			mat = AssetManager::GetAsset<MaterialAsset>(AssetManager::CoreAssetIDs::MATERIAL);
		}

		asset.materials.push_back(mat);
	}

	AssetManager::Get().mesh_buffer_manager.LoadMeshFromData(&asset, mesh_data);
	return &asset;
}


MaterialAsset* AssetLoader::DeserializeMaterial(const std::string& filepath) {
	SNK_ASSERT(filepath.ends_with(".mat"));
	std::vector<std::byte> data;
	files::ReadBinaryFile(filepath, data);
	if (data.empty()) {
		SNK_CORE_ERROR("DeserializeMaterial failed, reading filepath '{}' gave no data", filepath);
		return nullptr;
	}

	ByteDeserializer d{ data.data(), data.size() };

	size_t uuid;
	d.Value(uuid);
	auto& asset = *AssetManager::CreateAsset<MaterialAsset>(uuid).get();
	asset.uuid = UUID(uuid);
	asset.filepath = filepath;

	d.Container(asset.name);
	d.Value(asset.albedo);
	d.Value(asset.emissive);
	d.Value(asset.roughness);
	d.Value(asset.metallic);
	d.Value(asset.ao);
	std::array<uint64_t, 5> texture_uuids;

	d.Value(texture_uuids[0]);
	d.Value(texture_uuids[1]);
	d.Value(texture_uuids[2]);
	d.Value(texture_uuids[3]);
	d.Value(texture_uuids[4]);

	asset.albedo_tex = texture_uuids[0] == UUID<uint64_t>::INVALID_UUID ? nullptr : AssetManager::GetAsset<Texture2DAsset>(texture_uuids[0]);
	asset.normal_tex = texture_uuids[1] == UUID<uint64_t>::INVALID_UUID ? nullptr : AssetManager::GetAsset<Texture2DAsset>(texture_uuids[1]);
	asset.roughness_tex = texture_uuids[2] == UUID<uint64_t>::INVALID_UUID ? nullptr : AssetManager::GetAsset<Texture2DAsset>(texture_uuids[2]);
	asset.metallic_tex = texture_uuids[3] == UUID<uint64_t>::INVALID_UUID ? nullptr : AssetManager::GetAsset<Texture2DAsset>(texture_uuids[3]);
	asset.ao_tex = texture_uuids[4] == UUID<uint64_t>::INVALID_UUID ? nullptr : AssetManager::GetAsset<Texture2DAsset>(texture_uuids[4]);
	
	return &asset;
}


void AssetLoader::SerializeStaticMeshAsset(const std::string& output_filepath, StaticMeshAsset& asset) {
	SNK_ASSERT(output_filepath.ends_with(".smesh"))
	nlohmann::json j;
	j["UUID"] = asset.uuid();
	j["DataUUID"] = asset.data->uuid();
	j["Name"] = asset.name;
	files::WriteTextFile(output_filepath, j.dump(4));
}

StaticMeshAsset* AssetLoader::DeserializeStaticMeshAsset(const std::string& input_filepath) {
	SNK_ASSERT(input_filepath.ends_with(".smesh"));
	std::string content = files::ReadTextFile(input_filepath);
	if (content.empty()) {
		SNK_CORE_ERROR("DeserializeStaticMeshAsset failed, reading file '{}' gave no data", input_filepath);
		return nullptr;
	}

	nlohmann::json j = nlohmann::json::parse(content);
	try {
		auto asset = AssetManager::CreateAsset<StaticMeshAsset>(j.at("UUID").template get<uint64_t>());
		asset->filepath = input_filepath;
		uint64_t data_uuid = j.at("DataUUID").template get<uint64_t>();
		asset->data = AssetManager::GetAsset<MeshDataAsset>(data_uuid);

		if (!asset->data) {
			SNK_CORE_ERROR("DeserializeStaticMeshAsset error linking MeshDataAsset '{}', UUID not found", data_uuid);
			asset->data = AssetManager::GetAsset<StaticMeshAsset>(AssetManager::CoreAssetIDs::SPHERE_MESH)->data;
		}

		asset->name = j.at("Name").template get<std::string>();

		return asset.get();
	}
	catch (std::exception& e) {
		SNK_CORE_ERROR("DeserializeStaticMeshAsset failed, error parsing json data: {}\n'{}'", e.what(), j.dump(4));
		return nullptr;
	}
}

bool AssetLoader::LoadTextureFromFile(AssetRef<Texture2DAsset> tex, vk::Format fmt) {
	int width, height, channels;
	// Force 4 channels as most GPUs only support these as samplers
	stbi_uc* pixels = stbi_load(tex->filepath.c_str(), &width, &height, &channels, 4);

	if (!pixels) {
		return false;
	}
	vk::DeviceSize image_size = width * height * 4;

	S_VkBuffer staging_buffer{};
	staging_buffer.CreateBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
	void* p_data = staging_buffer.Map();
	memcpy(p_data, pixels, (size_t)image_size);
	staging_buffer.Unmap();

	stbi_image_free(pixels);

	Image2DSpec spec;
	spec.format = fmt;
	spec.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc;
	spec.size.x = width;
	spec.size.y = height;
	spec.tiling = vk::ImageTiling::eOptimal;
	spec.aspect_flags = vk::ImageAspectFlagBits::eColor;
	spec.mip_levels = static_cast<unsigned>(glm::floor(glm::log2(glm::max((float)spec.size.x, (float)spec.size.y)))) + 1;

	tex->image.SetSpec(spec);
	tex->image.CreateImage();

	tex->image.TransitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 0, spec.mip_levels);
	CopyBufferToImage(staging_buffer.buffer, tex->image.GetImage(), width, height);
	tex->image.GenerateMipmaps(vk::ImageLayout::eTransferDstOptimal);
	tex->image.TransitionImageLayout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 0, spec.mip_levels);

	tex->image.CreateImageView();
	tex->image.CreateSampler();

	AssetManager::Get().m_global_tex_buffer_manager.RegisterTexture(tex);

	return true;
}

std::unique_ptr<MeshData> AssetLoader::LoadMeshDataFromRawFile(const std::string& filepath, bool load_materials_and_textures) {
	auto p_data = std::make_unique<MeshData>();
	Assimp::Importer importer;

	const aiScene* p_scene = importer.ReadFile(filepath,
		aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_CalcTangentSpace);

	if (!p_scene) {
		SNK_CORE_ERROR("Failed to import mesh from filepath '{}' : '{}'", filepath, importer.GetErrorString());
		return nullptr;
	}

	p_data->num_vertices = 0;
	p_data->num_indices = 0;

	p_data->submeshes.resize(p_scene->mNumMeshes);
	for (size_t i = 0; i < p_scene->mNumMeshes; i++) {
		auto* p_submesh = p_scene->mMeshes[i];
		Submesh& sm = p_data->submeshes[i];
		sm.base_index = p_data->num_indices;
		sm.base_vertex = p_data->num_vertices;
		sm.num_indices = p_submesh->mNumFaces * 3;
		sm.material_index = p_submesh->mMaterialIndex;

		p_data->num_vertices += p_submesh->mNumVertices;
		p_data->num_indices += p_submesh->mNumFaces * 3;
	}

	p_data->positions = new aiVector3D[p_data->num_vertices];
	p_data->normals = new aiVector3D[p_data->num_vertices];
	p_data->tangents = new aiVector3D[p_data->num_vertices];
	p_data->tex_coords = new aiVector2D[p_data->num_vertices];
	p_data->indices = new unsigned[p_data->num_indices];

	std::byte* p_data_pos = reinterpret_cast<std::byte*>(p_data->positions);
	std::byte* p_data_norm = reinterpret_cast<std::byte*>(p_data->normals);
	std::byte* p_data_index = reinterpret_cast<std::byte*>(p_data->indices);
	std::byte* p_data_tex_coord = reinterpret_cast<std::byte*>(p_data->tex_coords);
	std::byte* p_data_tangent = reinterpret_cast<std::byte*>(p_data->tangents);

	size_t current_vert_offset = 0;
	unsigned indices_offset = 0;

	for (size_t i = 0; i < p_scene->mNumMeshes; i++) {
		auto& submesh = *p_scene->mMeshes[i];
		memcpy(p_data_pos + current_vert_offset * sizeof(aiVector3D), submesh.mVertices, submesh.mNumVertices * sizeof(aiVector3D));
		memcpy(p_data_norm + current_vert_offset * sizeof(aiVector3D), submesh.mNormals, submesh.mNumVertices * sizeof(aiVector3D));
		memcpy(p_data_tangent + current_vert_offset * sizeof(aiVector3D), submesh.mTangents, submesh.mNumVertices * sizeof(aiVector3D));
		current_vert_offset += submesh.mNumVertices;

		for (size_t j = 0; j < submesh.mNumFaces; j++) {
			aiFace& face = submesh.mFaces[j];
			memcpy(p_data_index + indices_offset * sizeof(unsigned), face.mIndices, sizeof(unsigned) * 3);
			indices_offset += 3;
		}

		for (size_t j = 0; j < submesh.mNumVertices; j++) {
			memcpy(p_data_tex_coord + j * sizeof(aiVector2D), &submesh.mTextureCoords[0][j], sizeof(aiVector2D));
		}
	}

	if (!load_materials_and_textures)
		return std::move(p_data);

	p_data->materials.resize(p_scene->mNumMaterials, UUID<uint64_t>::INVALID_UUID);

	if (p_data->materials.empty()) {
		p_data->materials.push_back(AssetManager::CoreAssetIDs::MATERIAL);
	}

	auto dir = files::GetFileDirectory(filepath);

	for (size_t i = 0; i < p_scene->mNumMaterials; i++) {
		aiMaterial* p_material = p_scene->mMaterials[i];
		AssetRef<MaterialAsset> material_asset = AssetManager::CreateAsset<MaterialAsset>();

		bool material_properties_set = false;

		if (auto [tex, is_newly_created] = CreateOrGetTextureFromMaterial(dir, aiTextureType_BASE_COLOR, p_material); tex) {
			material_asset->albedo_tex = tex;
			if (is_newly_created) p_data->textures.push_back(tex->uuid());
		}
		if (auto [tex, is_newly_created] = CreateOrGetTextureFromMaterial(dir, aiTextureType_NORMALS, p_material); tex) {
			material_asset->normal_tex = tex;
			if (is_newly_created) p_data->textures.push_back(tex->uuid());
		}
		if (auto [tex, is_newly_created] = CreateOrGetTextureFromMaterial(dir, aiTextureType_DIFFUSE_ROUGHNESS, p_material); tex) {
			material_asset->roughness_tex = tex;
			if (is_newly_created) p_data->textures.push_back(tex->uuid());
		}
		if (auto [tex, is_newly_created] = CreateOrGetTextureFromMaterial(dir, aiTextureType_METALNESS, p_material); tex) {
			material_asset->metallic_tex = tex;
			if (is_newly_created) p_data->textures.push_back(tex->uuid());
		}
		if (auto [tex, is_newly_created] = CreateOrGetTextureFromMaterial(dir, aiTextureType_AMBIENT_OCCLUSION, p_material); tex) {
			material_asset->ao_tex = tex;
			if (is_newly_created) p_data->textures.push_back(tex->uuid());
		}


		aiColor3D base_col(1.f, 1.f, 1.f);
		if (p_material->Get(AI_MATKEY_BASE_COLOR, base_col) == aiReturn_SUCCESS) {
			material_asset->albedo = { base_col.r, base_col.g, base_col.b };
			material_properties_set = true;
		}

		float roughness;
		float metallic;
		if (p_material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == aiReturn_SUCCESS) {
			material_asset->roughness = roughness;
			material_properties_set = true;
		}

		if (p_material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == aiReturn_SUCCESS) {
			material_asset->metallic = metallic;
			material_properties_set = true;
		}

		if (material_properties_set) {
			p_data->materials.push_back(material_asset->uuid());
		}
		else {
			material_asset = AssetManager::GetAsset<MaterialAsset>(AssetManager::CoreAssetIDs::MATERIAL);
			AssetManager::DeleteAsset(material_asset->uuid());
		}

		p_data->materials[i] = material_asset->uuid();
	}


	return std::move(p_data);
}

//void AssetLoader::LoadMeshFromData(AssetRef<MeshDataAsset> mesh_data_asset, MeshData& data) {
//	mesh_data_asset->submeshes = data.submeshes;
//	mesh_data_asset->num_indices = data.num_indices;
//	mesh_data_asset->num_vertices = data.num_vertices;
//
//	mesh_data_asset->position_buf.CreateBuffer(data.num_vertices * sizeof(aiVector3D),
//		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);
//
//	mesh_data_asset->normal_buf.CreateBuffer(data.num_vertices * sizeof(aiVector3D),
//		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress);
//
//	mesh_data_asset->index_buf.CreateBuffer(data.num_indices * sizeof(uint32_t),
//		vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);
//
//	mesh_data_asset->tex_coord_buf.CreateBuffer(data.num_vertices * sizeof(aiVector2D),
//		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress);
//
//	mesh_data_asset->tangent_buf.CreateBuffer(data.num_vertices * sizeof(aiVector3D),
//		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress);
//
//	S_VkBuffer staging_buf_pos;
//	staging_buf_pos.CreateBuffer(data.num_vertices * sizeof(aiVector3D),
//		vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
//
//	S_VkBuffer staging_buf_norm;
//	staging_buf_norm.CreateBuffer(data.num_vertices * sizeof(aiVector3D),
//		vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
//
//	S_VkBuffer staging_buf_index;
//	staging_buf_index.CreateBuffer(data.num_indices * sizeof(unsigned),
//		vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
//
//	S_VkBuffer staging_buf_tex_coord;
//	staging_buf_tex_coord.CreateBuffer(data.num_vertices * sizeof(aiVector2D),
//		vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
//
//	S_VkBuffer staging_buf_tangent;
//	staging_buf_tangent.CreateBuffer(data.num_vertices * sizeof(aiVector3D),
//		vk::BufferUsageFlagBits::eTransferSrc, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
//
//	std::byte* p_data_pos = reinterpret_cast<std::byte*>(staging_buf_pos.Map());
//	std::byte* p_data_norm = reinterpret_cast<std::byte*>(staging_buf_norm.Map());
//	std::byte* p_data_index = reinterpret_cast<std::byte*>(staging_buf_index.Map());
//	std::byte* p_data_tex_coord = reinterpret_cast<std::byte*>(staging_buf_tex_coord.Map());
//	std::byte* p_data_tangent = reinterpret_cast<std::byte*>(staging_buf_tangent.Map());
//
//
//	memcpy(p_data_pos, data.positions, data.num_vertices * sizeof(aiVector3D));
//	memcpy(p_data_norm, data.normals, data.num_vertices * sizeof(aiVector3D));
//	memcpy(p_data_tangent, data.tangents, data.num_vertices * sizeof(aiVector3D));
//	memcpy(p_data_tex_coord, data.tex_coords, data.num_vertices * sizeof(aiVector3D));
//	memcpy(p_data_index, data.indices, data.num_indices * sizeof(unsigned));
//	
//
//	CopyBuffer(staging_buf_pos.buffer, mesh_data_asset->position_buf.buffer, data.num_vertices * sizeof(aiVector3D));
//	CopyBuffer(staging_buf_norm.buffer, mesh_data_asset->normal_buf.buffer, data.num_vertices * sizeof(aiVector3D));
//	CopyBuffer(staging_buf_index.buffer, mesh_data_asset->index_buf.buffer, data.num_indices * sizeof(unsigned));
//	CopyBuffer(staging_buf_tex_coord.buffer, mesh_data_asset->tex_coord_buf.buffer, data.num_vertices * sizeof(aiVector2D));
//	CopyBuffer(staging_buf_tangent.buffer, mesh_data_asset->tangent_buf.buffer, data.num_vertices * sizeof(aiVector3D));
//
//	staging_buf_pos.Unmap();
//	staging_buf_index.Unmap();
//	staging_buf_norm.Unmap();
//	staging_buf_tex_coord.Unmap();
//	staging_buf_tangent.Unmap();
//
//	for (auto mat_uuid : data.materials) {
//		auto mat = AssetManager::GetAsset<MaterialAsset>(mat_uuid);
//		if (!mat) {
//			SNK_CORE_ERROR("LoadMeshFromData error: attempted to load material uuid '{}' which doesn't exist", mat_uuid);
//			mat = AssetManager::GetAsset<MaterialAsset>(AssetManager::CoreAssetIDs::MATERIAL);
//		}
//		mesh_data_asset->materials.push_back(mat);
//	}
//
//}

std::pair<AssetRef<Texture2DAsset>, bool>  AssetLoader::CreateOrGetTextureFromMaterial(const std::string& dir, aiTextureType type, aiMaterial* p_material) {
	AssetRef<Texture2DAsset> tex = nullptr;

	if (p_material->GetTextureCount(type) > 0) {
		aiString path;

		if (p_material->GetTexture(type, 0, &path, NULL, NULL, NULL, NULL, NULL) == aiReturn_SUCCESS) {
			std::string p(path.data);
			std::string full_path;

			if (p.starts_with(".\\"))
				p = p.substr(2, p.size() - 2);

			full_path = dir + "\\" + p;

			if (auto existing = AssetManager::GetAsset<Texture2DAsset>(full_path)) {
				return { existing, false };
			}

			if (!files::PathExists(full_path)) {
				SNK_CORE_ERROR("CreateOrGetTextureFromMaterial failed, invalid path '{}'", full_path);
				return { nullptr, false };
			}

			tex = AssetManager::CreateAsset<Texture2DAsset>();
			vk::Format fmt = type == aiTextureType_BASE_COLOR ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
			tex->filepath = full_path;
			AssetLoader::LoadTextureFromFile(tex, fmt);
		}
	}

	return { tex, true };
}
