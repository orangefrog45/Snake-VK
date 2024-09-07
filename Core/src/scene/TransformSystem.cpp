#include "pch/pch.h"
#include "scene/TransformSystem.h"
#include "util/ExtraMath.h"
#include "scene/Entity.h"
#include "scene/Scene.h"

using namespace SNAKE;

void TransformSystem::OnSystemAdd() {
	m_transform_event_listener.callback = [this](Event const* _event) {
		auto* p_casted = dynamic_cast<ComponentEvent<TransformComponent> const*>(_event);
		p_casted->p_component->mp_system = this;
		};

	m_relationship_event_listener.callback = [this](Event const* _event) {
		auto* p_casted = dynamic_cast<ComponentEvent<RelationshipComponent> const*>(_event);
		if (p_casted->event_type == ComponentEventType::UPDATED)
			RebuildMatrix(p_casted->p_component->GetEntity()->GetComponent<TransformComponent>());
		};

	EventManagerG::RegisterListener<ComponentEvent<TransformComponent>>(m_transform_event_listener);
	EventManagerG::RegisterListener<ComponentEvent<RelationshipComponent>>(m_relationship_event_listener);
};

TransformComponent* TransformSystem::GetParent(TransformComponent* p_comp) {
	if (auto parent = p_comp->GetEntity()->GetParent(); parent != entt::null)
		return p_scene->GetEntity(parent)->GetComponent<TransformComponent>();
	else
		return nullptr;
}

void TransformSystem::UpdateAbsTransforms(TransformComponent* p_comp) {
	TransformComponent* p_parent_transform = GetParent(p_comp);

	p_comp->m_abs_scale = p_comp->m_scale;
	p_comp->m_abs_orientation = p_comp->m_orientation;
	p_comp->m_abs_pos = glm::vec3(p_comp->m_transform[3][0], p_comp->m_transform[3][1], p_comp->m_transform[3][2]);

	if (p_comp->m_is_absolute)
		return;

	while (p_parent_transform) {
		p_comp->m_abs_orientation += p_parent_transform->m_orientation;
		p_comp->m_abs_scale *= p_parent_transform->m_scale;

		p_parent_transform = GetParent(p_parent_transform);
	}
}

void TransformSystem::RebuildMatrix(TransformComponent* p_comp) {
	auto* p_parent = GetParent(p_comp);

	auto& transform = p_comp->m_transform;
	auto& orientation = p_comp->m_orientation;
	auto& pos = p_comp->m_pos;
	auto& scale = p_comp->m_scale;

	if (p_comp->m_is_absolute || !p_parent) {
		transform = ExtraMath::Rotate3D(orientation.x, orientation.y, orientation.z);
		transform[0][0] *= scale.x;
		transform[0][1] *= scale.x;
		transform[0][2] *= scale.x;

		transform[1][0] *= scale.y;
		transform[1][1] *= scale.y;
		transform[1][2] *= scale.y;

		transform[2][0] *= scale.z;
		transform[2][1] *= scale.z;
		transform[2][2] *= scale.z;

		transform[3][0] = pos.x;
		transform[3][1] = pos.y;
		transform[3][2] = pos.z;
		transform[3][3] = 1.0;
	}
	else {
		glm::vec3 p_s = p_parent->GetAbsScale();
		float inv_parent_scale_det = 1.f / (p_s.x * p_s.y * p_s.z);
		glm::vec3 total_scale = scale * p_s;
		glm::vec3 inv_scale{ p_s.y * p_s.z * inv_parent_scale_det, p_s.x * p_s.z * inv_parent_scale_det, p_s.y * p_s.x * inv_parent_scale_det };

		transform = ExtraMath::Rotate3D(orientation.x, orientation.y, orientation.z);
		// Apply total_scale whilst undoing parent scaling transforms to prevent shearing
		transform[0][0] *= total_scale.x * inv_scale.x;
		transform[0][1] *= total_scale.x * inv_scale.y;
		transform[0][2] *= total_scale.x * inv_scale.z;

		transform[1][0] *= total_scale.y * inv_scale.x;
		transform[1][1] *= total_scale.y * inv_scale.y;
		transform[1][2] *= total_scale.y * inv_scale.z;

		transform[2][0] *= total_scale.z * inv_scale.x;
		transform[2][1] *= total_scale.z * inv_scale.y;
		transform[2][2] *= total_scale.z * inv_scale.z;

		transform[3][0] = pos.x;
		transform[3][1] = pos.y;
		transform[3][2] = pos.z;
		transform[3][3] = 1.0;

		transform = p_parent->GetMatrix() * transform;
	}

	p_comp->forward = glm::normalize(glm::vec3(-transform[2][0], -transform[2][1], -transform[2][2]));
	p_comp->right = glm::normalize(glm::cross(p_comp->forward, p_comp->g_up));
	p_comp->up = glm::normalize(glm::cross(p_comp->right, p_comp->forward));

	UpdateAbsTransforms(p_comp);

	UpdateChildren(p_comp);

	EventManagerG::DispatchEvent(ComponentEvent<TransformComponent>(p_comp, ComponentEventType::UPDATED));
}

void TransformSystem::UpdateChildren(TransformComponent* p_comp) {
	// For now simplest approach, will optimize a bit later if it becomes a performance problem
	p_comp->GetEntity()->ForEachLevelOneChild([&](entt::entity child_handle) {
		RebuildMatrix(p_scene->GetEntity(child_handle)->GetComponent<TransformComponent>());
	});
}


void TransformSystem::SetAbsolutePosition(TransformComponent* p_comp, glm::vec3 pos) {
	glm::vec3 final_pos = pos;
	if (auto* p_parent = GetParent(p_comp); p_parent && !p_comp->m_is_absolute) {
		final_pos = glm::inverse(p_parent->GetMatrix()) * glm::vec4(pos, 1.0);
	}

	p_comp->SetPosition(final_pos);
}

void TransformSystem::OnSceneShutdown() {
	EventManagerG::DeregisterListener(m_transform_event_listener);
	EventManagerG::DeregisterListener(m_relationship_event_listener);
};