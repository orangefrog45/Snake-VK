#include "pch/pch.h"

#include "components/TransformComponent.h"
#include "util/ExtraMath.h"
#include "scene/TransformSystem.h"


namespace SNAKE {
	void TransformComponent::LookAt(glm::vec3 t_pos, glm::vec3 t_up) {
		glm::vec3 euler_angles = glm::degrees(glm::eulerAngles(glm::quatLookAt(glm::normalize(t_pos - m_abs_pos), glm::normalize(t_up))));
		SetAbsoluteOrientation(euler_angles);
	}

	void TransformComponent::RebuildMatrix([[maybe_unused]] UpdateType type) {
		mp_system->RebuildMatrix(this);
	}

	void TransformComponent::SetAbsolutePosition(glm::vec3 pos) {
		mp_system->SetAbsolutePosition(this, pos);
	}

}