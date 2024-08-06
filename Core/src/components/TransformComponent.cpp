#include "pch/pch.h"

#include "components/TransformComponent.h"
#include "events/EventManager.h"
#include "scene/Entity.h"
#include "util/ExtraMath.h"


namespace SNAKE {

	void TransformComponent::UpdateAbsTransforms() {
		TransformComponent* p_parent_transform = GetParent();

		m_abs_scale = m_scale;
		m_abs_orientation = m_orientation;
		m_abs_pos = glm::vec3(m_transform[3][0], m_transform[3][1], m_transform[3][2]);

		if (m_is_absolute)
			return;

		while (p_parent_transform) {
			m_abs_orientation += p_parent_transform->m_orientation;
			m_abs_scale *= p_parent_transform->m_scale;

			p_parent_transform = p_parent_transform->GetParent();
		}
	}


	void TransformComponent::LookAt(glm::vec3 t_pos, glm::vec3 t_up) {
		glm::vec3 euler_angles = glm::degrees(glm::eulerAngles(glm::quatLookAt(glm::normalize(t_pos - m_abs_pos), glm::normalize(t_up))));
		SetAbsoluteOrientation(euler_angles);
	}


	TransformComponent* TransformComponent::GetParent() {
		//return m_parent_handle == entt::null ? nullptr : &GetEntity()->GetRegistry()->get<TransformComponent>(m_parent_handle);
		return nullptr;
	}

	void TransformComponent::RebuildMatrix(UpdateType type) {
		//ORNG_TRACY_PROFILE;

		auto* p_parent = GetParent();
		if (m_is_absolute || !p_parent) {
			m_transform = ExtraMath::Rotate3D(m_orientation.x, m_orientation.y, m_orientation.z);
			m_transform[0][0] *= m_scale.x;
			m_transform[0][1] *= m_scale.x;
			m_transform[0][2] *= m_scale.x;

			m_transform[1][0] *= m_scale.y;
			m_transform[1][1] *= m_scale.y;
			m_transform[1][2] *= m_scale.y;

			m_transform[2][0] *= m_scale.z;
			m_transform[2][1] *= m_scale.z;
			m_transform[2][2] *= m_scale.z;

			m_transform[3][0] = m_pos.x;
			m_transform[3][1] = m_pos.y;
			m_transform[3][2] = m_pos.z;
			m_transform[3][3] = 1.0;
		}
		else {
			glm::vec3 p_s = p_parent->GetAbsScale();
			float inv_parent_scale_det = 1.f / (p_s.x * p_s.y * p_s.z);
			glm::vec3 total_scale = m_scale * p_s;
			glm::vec3 inv_scale{ p_s.y * p_s.z * inv_parent_scale_det, p_s.x * p_s.z * inv_parent_scale_det, p_s.y * p_s.x * inv_parent_scale_det };

			m_transform = ExtraMath::Rotate3D(m_orientation.x, m_orientation.y, m_orientation.z);
			// Apply total_scale whilst undoing parent scaling transforms to prevent shearing
			m_transform[0][0] *= total_scale.x * inv_scale.x;
			m_transform[0][1] *= total_scale.x * inv_scale.y;
			m_transform[0][2] *= total_scale.x * inv_scale.z;

			m_transform[1][0] *= total_scale.y * inv_scale.x;
			m_transform[1][1] *= total_scale.y * inv_scale.y;
			m_transform[1][2] *= total_scale.y * inv_scale.z;

			m_transform[2][0] *= total_scale.z * inv_scale.x;
			m_transform[2][1] *= total_scale.z * inv_scale.y;
			m_transform[2][2] *= total_scale.z * inv_scale.z;

			m_transform[3][0] = m_pos.x;
			m_transform[3][1] = m_pos.y;
			m_transform[3][2] = m_pos.z;
			m_transform[3][3] = 1.0;

			m_transform = p_parent->GetMatrix() * m_transform;
		}

		forward = glm::normalize(glm::vec3(-m_transform[2][0], -m_transform[2][1], -m_transform[2][2]));
		right = glm::normalize(glm::cross(forward, g_up));
		up = glm::normalize(glm::cross(right, forward));

		UpdateAbsTransforms();

		//if (GetEntity()) {
		//	Events::ECS_Event<TransformComponent> e_event{ Events::ECS_EventType::COMP_UPDATED, this, type };
		//	Events::EventManager::DispatchEvent(e_event);
		//}
	}
}