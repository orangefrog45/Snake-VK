#pragma once
#include "System.h"
#include "events/EventManager.h"
#include "components/Component.h"
#include "components/TransformComponent.h"

namespace SNAKE {
	class TransformSystem : public System {
	public:
		void OnSystemAdd() override;

		void OnSystemRemove() override;

		TransformComponent* GetParent(TransformComponent* p_comp);

		void RebuildMatrix(TransformComponent* p_comp);
		
		void UpdateAbsTransforms(TransformComponent* p_comp);

		void UpdateChildren(TransformComponent* p_comp);

		void SetAbsolutePosition(TransformComponent* p_comp, glm::vec3 pos);
	private:
		EventListener m_transform_event_listener;
		EventListener m_relationship_event_listener;
	};
}