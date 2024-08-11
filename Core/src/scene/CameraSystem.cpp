#include "scene/CameraSystem.h"
#include "scene/Entity.h"
#include "components/CameraComponent.h"

namespace SNAKE {
	void CameraSystem::OnSystemAdd() {
		m_cam_update_listener.callback = [this](Event const* p_event) {
			auto* p_casted = dynamic_cast<ComponentEvent<CameraComponent> const*>(p_event);

			if (p_casted->event_type == ComponentEventType::UPDATED) {
				if (p_active_cam_ent)
					p_active_cam_ent->GetComponent<CameraComponent>()->m_is_active = false;

				p_casted->p_component->m_is_active = true;
				p_active_cam_ent = p_casted->p_component->GetEntity();
			}
			else if (p_casted->event_type == ComponentEventType::REMOVED) {
				p_active_cam_ent = nullptr;
			}
			};

		EventManagerG::RegisterListener<ComponentEvent<CameraComponent>>(m_cam_update_listener);
	}

	CameraComponent* CameraSystem::GetActiveCam() {
		return p_active_cam_ent ? p_active_cam_ent->GetComponent<CameraComponent>() : nullptr;
	}
}