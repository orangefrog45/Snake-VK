#include "pch/pch.h"
#include "rendering/MeshBufferManager.h"
#include "assets/MeshData.h"
#include "assets/AssetManager.h"
#include "rendering/Raytracing.h"

using namespace SNAKE;

void MeshBufferManager::Init() {
	m_position_buf.CreateBuffer(256, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress |
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc);
	m_normal_buf.CreateBuffer(256, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress |
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc);
	m_tangent_buf.CreateBuffer(256, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress |
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc);
	m_tex_coord_buf.CreateBuffer(256, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress |
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc);
	m_index_buf.CreateBuffer(256, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress |
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc);
}

void MeshBufferManager::LoadMeshFromData(MeshDataAsset* p_mesh_data_asset, MeshData& data) {
	p_mesh_data_asset->submeshes = data.submeshes;
	p_mesh_data_asset->num_indices = data.num_indices;
	p_mesh_data_asset->num_vertices = data.num_vertices;

	p_mesh_data_asset->submesh_blas_array.resize(p_mesh_data_asset->submeshes.size(), nullptr);
	for (size_t i = 0; i < p_mesh_data_asset->submeshes.size(); i++) {
		p_mesh_data_asset->submesh_blas_array[i] = new BLAS();
		p_mesh_data_asset->submesh_blas_array[i]->GenerateFromMeshData(data, (uint32_t)i);
	}

	S_VkBuffer staging_buf{};

	uint32_t prev_total_indices = m_total_indices;
	uint32_t prev_total_vertices = m_total_vertices;
	m_total_indices += data.num_indices;
	m_total_vertices += data.num_vertices;

	auto alignment = VkContext::GetPhysicalDevice().buffer_properties.descriptorBufferOffsetAlignment;
	auto aligned_vertex_size = aligned_size(m_total_vertices * sizeof(aiVector3D), alignment);

	// Create and fill staging buffer with mesh data
	staging_buf.CreateBuffer(glm::max(data.num_vertices * sizeof(aiVector3D), data.num_indices * sizeof(unsigned)),
		vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
	auto* p_staging_data = staging_buf.Map();
	memcpy(p_staging_data, data.positions, data.num_vertices * sizeof(aiVector3D));

	m_position_buf.Resize(aligned_vertex_size);
	m_normal_buf.Resize(aligned_vertex_size);
	m_tangent_buf.Resize(aligned_vertex_size);
	m_tex_coord_buf.Resize(aligned_size(m_total_vertices * sizeof(aiVector2D), alignment));
	m_index_buf.Resize(aligned_size(m_total_indices * sizeof(unsigned), alignment));


	// Copy data from newly loaded mesh into new buffer
	memcpy(p_staging_data, data.positions, data.num_vertices * sizeof(aiVector3D));
	CopyBuffer(staging_buf.buffer, m_position_buf.buffer, data.num_vertices * sizeof(aiVector3D), 0, prev_total_vertices * sizeof(aiVector3D));

	memcpy(p_staging_data, data.normals, data.num_vertices * sizeof(aiVector3D));
	CopyBuffer(staging_buf.buffer, m_normal_buf.buffer, data.num_vertices * sizeof(aiVector3D), 0, prev_total_vertices * sizeof(aiVector3D));

	memcpy(p_staging_data, data.tangents, data.num_vertices * sizeof(aiVector3D));
	CopyBuffer(staging_buf.buffer, m_tangent_buf.buffer, data.num_vertices * sizeof(aiVector3D), 0, prev_total_vertices * sizeof(aiVector3D));

	memcpy(p_staging_data, data.tex_coords, data.num_vertices * sizeof(aiVector2D));
	CopyBuffer(staging_buf.buffer, m_tex_coord_buf.buffer, data.num_vertices * sizeof(aiVector2D), 0, prev_total_vertices * sizeof(aiVector2D));

	memcpy(p_staging_data, data.indices, data.num_indices * sizeof(unsigned));
	CopyBuffer(staging_buf.buffer, m_index_buf.buffer, data.num_indices * sizeof(unsigned), 0, prev_total_indices * sizeof(unsigned));
	
	staging_buf.Unmap();

	for (auto mat_uuid : data.materials) {
		auto mat = AssetManager::GetAsset<MaterialAsset>(mat_uuid);
		if (!mat) {
			SNK_CORE_ERROR("LoadMeshFromData error: attempted to load material uuid '{}' which doesn't exist", mat_uuid);
			mat = AssetManager::GetAsset<MaterialAsset>(AssetManager::CoreAssetIDs::MATERIAL);
		}
		p_mesh_data_asset->materials.push_back(mat);
	}

	auto& entry = m_entries[p_mesh_data_asset];
	entry.data_start_indices_idx = prev_total_indices;
	entry.data_start_vertex_idx = prev_total_vertices;
	entry.num_indices = data.num_indices;
	entry.num_indices = data.num_vertices;
}



void MeshBufferManager::UnloadMesh([[maybe_unused]] MeshDataAsset* p_mesh_data_asset) {
	// TODO: Implement
}