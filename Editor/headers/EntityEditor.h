#pragma once
#include "scene/Scene.h"
#include "components/Components.h"

namespace SNAKE {
	class EntityEditor {
	public:
		EntityEditor(Scene* _scene) : p_scene(_scene) {}

		// Returns true if any entity/component has been modified
		bool RenderImGui();

		bool TransformCompEditor(TransformComponent* p_comp);

		bool StaticMeshCompEditor(StaticMeshComponent* p_comp);

		void SelectEntity(Entity* p_ent);

		void DeselectEntity(Entity* p_ent);

		void DestroyEntity(Entity* p_ent);
		
		Scene* p_scene = nullptr;
	private:
		Entity* mp_active_entity = nullptr;

	};
}