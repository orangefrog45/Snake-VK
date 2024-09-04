#pragma once
#include "pch/pch.h"
#include "rendering/MeshBufferManager.h"
#include "assets/MeshData.h"
#include "assets/AssetManager.h"
#include "rendering/Raytracing.h"

using namespace SNAKE;

void MeshBufferManager::LoadMeshFromData(MeshDataAsset* p_mesh_data_asset, MeshData& data) {
	p_mesh_data_asset->p_blas = new BLAS();
	p_mesh_data_asset->p_blas->GenerateFromMeshData(data);

	p_mesh_data_asset->submeshes = data.submeshes;
	p_mesh_data_asset->num_indices = data.num_indices;
	p_mesh_data_asset->num_vertices = data.num_vertices;

	std::unique_ptr<S_VkBuffer> p_new_position_buf = std::make_unique<S_VkBuffer>();
	std::unique_ptr<S_VkBuffer> p_new_normal_buf = std::make_unique<S_VkBuffer>();
	std::unique_ptr<S_VkBuffer> p_new_tangent_buf = std::make_unique<S_VkBuffer>();
	std::unique_ptr<S_VkBuffer> p_new_tex_coord_buf = std::make_unique<S_VkBuffer>();
	std::unique_ptr<S_VkBuffer> p_new_index_buf = std::make_unique<S_VkBuffer>();

	S_VkBuffer staging_buf{};

	uint32_t prev_total_indices = m_total_indices;
	uint32_t prev_total_vertices = m_total_vertices;
	m_total_indices += data.num_indices;
	m_total_vertices += data.num_vertices;

	p_new_position_buf->CreateBuffer(aligned_size(m_total_vertices * sizeof(aiVector3D), 256),
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress |
		vk::BufferUsageFlagBits::eStorageBuffer);

	p_new_normal_buf->CreateBuffer(m_total_vertices * sizeof(aiVector3D),
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | 
		vk::BufferUsageFlagBits::eStorageBuffer);

	p_new_tangent_buf->CreateBuffer(m_total_vertices * sizeof(aiVector3D),
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress |
		vk::BufferUsageFlagBits::eStorageBuffer);

	p_new_tex_coord_buf->CreateBuffer(m_total_vertices * sizeof(aiVector2D),
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress |
		vk::BufferUsageFlagBits::eStorageBuffer);

	p_new_index_buf->CreateBuffer(m_total_indices * sizeof(unsigned),
		vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress |
		vk::BufferUsageFlagBits::eStorageBuffer);

	// Ensure no rendering commands are running on the gpu while buffers are grown
	SNK_CHECK_VK_RESULT(VkContext::GetLogicalDevice().device->waitIdle());

	// Create and fill staging buffer with mesh data
	staging_buf.CreateBuffer(glm::max(data.num_vertices * sizeof(aiVector3D), data.num_indices * sizeof(unsigned)),
		vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst, VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
	auto* p_staging_data = staging_buf.Map();
	memcpy(p_staging_data, data.positions, data.num_vertices * sizeof(aiVector3D));

	// Copy old data from old to new buffer
	if (mp_position_buf)
		CopyBuffer(mp_position_buf->buffer, p_new_position_buf->buffer, prev_total_vertices * sizeof(aiVector3D));

	// Copy data from newly loaded mesh into new buffer
	CopyBuffer(staging_buf.buffer, p_new_position_buf->buffer, data.num_vertices * sizeof(aiVector3D), 0, prev_total_vertices * sizeof(aiVector3D));

	// Buffers swapped out one by one to prevent vram spike from duplicating all buffers simultaneously
	mp_position_buf = std::move(p_new_position_buf);

	// Repeating process for other buffers
	memcpy(p_staging_data, data.normals, data.num_vertices * sizeof(aiVector3D));
	if (mp_normal_buf)
		CopyBuffer(mp_normal_buf->buffer, p_new_normal_buf->buffer, prev_total_indices * sizeof(aiVector3D));
	CopyBuffer(staging_buf.buffer, p_new_normal_buf->buffer, data.num_vertices * sizeof(aiVector3D), 0, prev_total_vertices * sizeof(aiVector3D));
	mp_normal_buf = std::move(p_new_normal_buf);

	memcpy(p_staging_data, data.tangents, data.num_vertices * sizeof(aiVector3D));
	if (mp_tangent_buf)
		CopyBuffer(mp_tangent_buf->buffer, p_new_tangent_buf->buffer, prev_total_indices * sizeof(aiVector3D));
	CopyBuffer(staging_buf.buffer, p_new_tangent_buf->buffer, data.num_vertices * sizeof(aiVector3D), 0, prev_total_vertices * sizeof(aiVector3D));
	mp_tangent_buf = std::move(p_new_tangent_buf);

	memcpy(p_staging_data, data.tex_coords, data.num_vertices * sizeof(aiVector2D));
	if (mp_tex_coord_buf)
		CopyBuffer(mp_tex_coord_buf->buffer, p_new_tex_coord_buf->buffer, prev_total_indices * sizeof(aiVector2D));
	CopyBuffer(staging_buf.buffer, p_new_tex_coord_buf->buffer, data.num_vertices * sizeof(aiVector2D), 0, prev_total_vertices * sizeof(aiVector2D));
	mp_tex_coord_buf = std::move(p_new_tex_coord_buf);

	memcpy(p_staging_data, data.indices, data.num_indices * sizeof(unsigned));
	if (mp_index_buf)
		CopyBuffer(mp_index_buf->buffer, p_new_index_buf->buffer, prev_total_indices * sizeof(unsigned));
	CopyBuffer(staging_buf.buffer, p_new_index_buf->buffer, data.num_indices * sizeof(unsigned), 0, prev_total_indices * sizeof(unsigned));
	mp_index_buf = std::move(p_new_index_buf);

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



void MeshBufferManager::UnloadMesh(MeshDataAsset* p_p_mesh_data_asset) {

}