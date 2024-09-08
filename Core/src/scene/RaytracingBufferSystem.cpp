#include "pch/pch.h"
#include "scene/RaytracingBufferSystem.h"
#include "assets/AssetManager.h"
#include "components/Components.h"
#include "scene/Scene.h"

using namespace SNAKE;

void RaytracingInstanceBufferSystem::OnSystemAdd() {
	m_mesh_event_listener.callback = [this](Event const* p_event) {
		auto* p_casted = dynamic_cast<ComponentEvent<StaticMeshComponent> const*>(p_event);
		auto* p_ent = p_casted->p_component->GetEntity();

		if (p_casted->event_type == ComponentEventType::UPDATED)
			m_instances_to_update.push_back(std::make_pair(p_ent->GetEnttHandle(), 0));
		else if (p_casted->event_type == ComponentEventType::ADDED) {
			p_ent->AddComponent<RaytracingInstanceBufferIdxComponent>(m_current_buffer_idx);
			m_instances_to_update.push_back(std::make_pair(p_ent->GetEnttHandle(), 0));

			// Allocate space for submeshes
			m_current_buffer_idx += (uint32_t)p_casted->p_component->GetMeshAsset()->data->submeshes.size();
		}
		};

	m_transform_event_listener.callback = [this](Event const* p_event) {
		auto* p_casted = dynamic_cast<ComponentEvent<TransformComponent> const*>(p_event);

		if (auto* p_ent = p_casted->p_component->GetEntity(); p_ent->GetComponent<StaticMeshComponent>()) {
			m_instances_to_update.push_back(std::make_pair(p_ent->GetEnttHandle(), 0));
		}
	};

	m_frame_start_event_listener.callback = [this]([[maybe_unused]] Event const* p_event) {
		UpdateBuffer(VkContext::GetCurrentFIF());
		};

	EventManagerG::RegisterListener<FrameStartEvent>(m_frame_start_event_listener);
	EventManagerG::RegisterListener<ComponentEvent<TransformComponent>>(m_transform_event_listener);
	EventManagerG::RegisterListener<ComponentEvent<StaticMeshComponent>>(m_mesh_event_listener);

	for (FrameInFlightIndex i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		m_storage_buffers[i].CreateBuffer(sizeof(InstanceData) * 4096, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
		m_storage_buffers[i].Map();
	}
};

void RaytracingInstanceBufferSystem::UpdateBuffer(FrameInFlightIndex idx) {
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
			.transform = reg.get<TransformComponent>(ent).GetMatrix(),
			.mesh_buffer_index_offset = mesh_buffer_entry_data.data_start_indices_idx,
			.mesh_buffer_vertex_offset = mesh_buffer_entry_data.data_start_vertex_idx,
			.flags = 0,
		};

		for (size_t j = 0; j < p_mesh_asset->data->submeshes.size(); j++) {
			auto& mat = *mesh_comp.GetMaterial(p_mesh_asset->data->submeshes[j].material_index);
			instance_data.material_idx = mat.GetGlobalBufferIndex();
			instance_data.mesh_buffer_vertex_offset = mesh_buffer_entry_data.data_start_vertex_idx + p_mesh_asset->data->submeshes[j].base_vertex;
			instance_data.mesh_buffer_index_offset = mesh_buffer_entry_data.data_start_indices_idx + p_mesh_asset->data->submeshes[j].base_index;
			memcpy(reinterpret_cast<std::byte*>(m_storage_buffers[idx].Map()) + (buf_idx + j) * sizeof(InstanceData),
				&instance_data, sizeof(InstanceData));
		}

		if (update_count == MAX_FRAMES_IN_FLIGHT) {
			m_instances_to_update.erase(m_instances_to_update.begin() + i);
			i--;
		}
	}
}

const S_VkBuffer& RaytracingInstanceBufferSystem::GetStorageBuffer(FrameInFlightIndex idx) {
	return m_storage_buffers[idx];
}