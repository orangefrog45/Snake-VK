#include "pch/pch.h"
#include "scene/RaytracingBufferSystem.h"
#include "assets/AssetManager.h"
#include "components/Components.h"
#include "scene/Scene.h"
#include "scene/TransformBufferSystem.h"

using namespace SNAKE;

void RaytracingInstanceBufferSystem::OnSystemAdd() {
	m_mesh_event_listener.callback = [this](Event const* p_event) {
		auto* p_casted = dynamic_cast<ComponentEvent<StaticMeshComponent> const*>(p_event);
		auto* p_ent = p_casted->p_component->GetEntity();

		// On either a mesh component being added or updated, just refit it to another slot in the buffer (submesh count may have changed so needs new slot)
		// TODO: This causes a lot of waste, track number of tombstones
		if (p_casted->event_type != ComponentEventType::REMOVED) {
			// Add or update
			p_ent->AddComponent<RaytracingInstanceBufferIdxComponent>(m_current_buffer_idx)->idx = m_current_buffer_idx;
			m_instances_to_update.push_back(std::make_pair(p_ent->GetEnttHandle(), 0));

			// Allocate space for submeshes
			m_current_buffer_idx += (uint32_t)p_casted->p_component->GetMeshAsset()->data->submeshes.size();
		} else {
			// Delete all references to this instance from the update queue
			unsigned deletion_count = 0;
			std::vector<uint32_t> deletion_indices;
			entt::entity deleted_entity_handle = p_ent->GetEnttHandle();

			for (uint32_t i = 0; i < m_instances_to_update.size(); i++) {
				if (m_instances_to_update[i].first == deleted_entity_handle)
					deletion_indices.push_back(i);
			}

			for (auto idx : deletion_indices) {
				m_instances_to_update.erase(m_instances_to_update.begin() + idx - deletion_count);
				deletion_count++;
			}
		}
		};

	m_transform_event_listener.callback = [this](Event const* p_event) {
		auto* p_casted = dynamic_cast<ComponentEvent<TransformComponent> const*>(p_event);

		if (auto* p_ent = p_casted->p_component->GetEntity(); p_ent->GetComponent<StaticMeshComponent>()) {
			m_instances_to_update.push_back(std::make_pair(p_ent->GetEnttHandle(), 0));
		}
	};

	m_frame_start_event_listener.callback = [this]([[maybe_unused]] Event const* p_event) {
		UpdateInstanceBuffer();
		UpdateEmissiveIdxBuffer();
		};

	EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_event_listener);
	EventManagerG::RegisterListener<ComponentEvent<TransformComponent>>(m_transform_event_listener);
	EventManagerG::RegisterListener<ComponentEvent<StaticMeshComponent>>(m_mesh_event_listener);

	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_storage_buffers_instances[i].CreateBuffer(sizeof(InstanceData) * 4096, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
		m_storage_buffers_emissive_indices[i].CreateBuffer(sizeof(InstanceData) * 4096, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
		m_storage_buffers_instances[i].Map();
		m_storage_buffers_emissive_indices[i].Map();
	}
};

void RaytracingInstanceBufferSystem::UpdateInstanceBuffer() {
	FrameInFlightIndex fif = VkContext::GetCurrentFIF();
	auto& mesh_buffer_manager = AssetManager::Get().mesh_buffer_manager;

	for (size_t i = 0; i < m_instances_to_update.size(); i++) {
		auto& [ent, update_count] = m_instances_to_update[i];
		update_count++;

		auto& reg = p_scene->GetRegistry();
		auto buf_idx = reg.get<RaytracingInstanceBufferIdxComponent>(ent).idx;

		auto& mesh_comp = reg.get<StaticMeshComponent>(ent);
		auto* p_mesh_asset = mesh_comp.GetMeshAsset();
		auto& mesh_buffer_entry_data = mesh_buffer_manager.GetEntryData(p_mesh_asset->data.get());

		InstanceData instance_data{
			.transform_idx = reg.get<TransformBufferIdxComponent>(ent).idx,
		};

		for (size_t j = 0; j < p_mesh_asset->data->submeshes.size(); j++) {
			auto& mat = *mesh_comp.GetMaterial(p_mesh_asset->data->submeshes[j].material_index);
			instance_data.material_idx = mat.GetGlobalBufferIndex();
			instance_data.mesh_buffer_vertex_offset = mesh_buffer_entry_data.data_start_vertex_idx + p_mesh_asset->data->submeshes[j].base_vertex;
			instance_data.mesh_buffer_index_offset = mesh_buffer_entry_data.data_start_indices_idx + p_mesh_asset->data->submeshes[j].base_index;
			instance_data.num_mesh_indices = p_mesh_asset->data->submeshes[j].num_indices;
			memcpy(reinterpret_cast<std::byte*>(m_storage_buffers_instances[fif].Map()) + (buf_idx + j) * sizeof(InstanceData),
				&instance_data, sizeof(InstanceData));
		}

		if (update_count == MAX_FRAMES_IN_FLIGHT) {
			m_instances_to_update.erase(m_instances_to_update.begin() + i);
			i--;
		}
	}
}

void RaytracingInstanceBufferSystem::UpdateEmissiveIdxBuffer() {
	FrameInFlightIndex fif = VkContext::GetCurrentFIF();
	auto& reg = p_scene->GetRegistry();
	uint32_t current_idx = 0;

	// Locate indices of any emissive object instances and store these indices in m_storage_buffers_emissive_indices
	for (auto [entity, rt_instance_comp] : p_scene->GetRegistry().view<RaytracingInstanceBufferIdxComponent>().each()) {
		auto& mesh = reg.get<StaticMeshComponent>(entity);
		auto& materials = mesh.GetMaterials();
		const auto& submeshes = mesh.GetMeshAsset()->data->submeshes;
		
		for (size_t i = 0; i < submeshes.size(); i++) {
			if (materials[submeshes[i].material_index]->flags & (uint32_t)MaterialAsset::MaterialFlagBits::EMISSIVE) {
				uint32_t emissive_instance_idx = rt_instance_comp.idx + (uint32_t)i;

				memcpy(reinterpret_cast<std::byte*>(m_storage_buffers_emissive_indices[fif].Map()) + (1 + current_idx) * sizeof(uint32_t),
					&emissive_instance_idx, sizeof(uint32_t));

				current_idx++;
			}
		}
	}

	memcpy(m_storage_buffers_emissive_indices[fif].Map(),
		&current_idx, sizeof(uint32_t));
}

const S_VkBuffer& RaytracingInstanceBufferSystem::GetInstanceStorageBuffer(FrameInFlightIndex idx) {
	return m_storage_buffers_instances[idx];
}

const S_VkBuffer& RaytracingInstanceBufferSystem::GetEmissiveIdxStorageBuffer(FrameInFlightIndex idx) {
	return m_storage_buffers_emissive_indices[idx];
}