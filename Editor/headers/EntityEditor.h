#pragma once
#include "scene/Scene.h"
#include "components/Components.h"

namespace SNAKE {
	class EntityEditor {
	public:
		EntityEditor(Scene* _scene) : p_scene(_scene) {}

		void Init(class AssetEditor* _asset_editor) {
			p_asset_editor = _asset_editor;
		}

		// Returns true if any entity/component has been modified
		bool RenderImGui();

		bool TransformCompEditor(TransformComponent* p_comp);

		bool StaticMeshCompEditor(StaticMeshComponent* p_comp);

		void SelectEntity(Entity* p_ent);

		void DeselectEntity(Entity* p_ent);

		void DestroyEntity(Entity* p_ent);

		Entity* GetSelectedEntity() {
			return mp_active_entity;
		}
		
		Scene* p_scene = nullptr;

		class AssetEditor* p_asset_editor = nullptr;
	private:
		Entity* mp_active_entity = nullptr;

	};
}