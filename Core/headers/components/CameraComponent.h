#include "Component.h"
#include "events/EventManager.h"
#include "events/EventsCommon.h"

namespace SNAKE {
	class CameraComponent : public Component {
	public:
		CameraComponent(Entity* p_entity) : Component(p_entity) {};

		float fov = 90.f;
		float z_near = 0.1f;
		float z_far = 2500.f;
		float aspect_ratio = 16.f / 9.f;

		glm::mat4 GetProjectionMatrix() {
			auto proj = glm::perspective(glm::radians(fov * 0.5f), aspect_ratio, z_near, z_far);
			proj[1][1] *= -1; // Flip y for vulkan
			return proj;
		}

		// Makes camera active in scene (rendering done from this cameras perspective)
		void MakeActive() {
			EventManagerG::DispatchEvent(ComponentEvent<CameraComponent>(this, ComponentEventType::UPDATED));
		}

	private:
		bool m_is_active = false;
		friend class CameraSystem;
	};
}