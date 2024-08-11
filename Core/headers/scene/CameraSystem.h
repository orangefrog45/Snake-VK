#include "System.h"
#include "events/EventManager.h"

namespace SNAKE {
	class CameraSystem : public System {
	public:
		void OnSystemAdd() override;
		
		class CameraComponent* GetActiveCam();

	private:
		class Entity* p_active_cam_ent = nullptr;
		EventListener m_cam_update_listener;
	};
}