#pragma once

#include "assets/MeshData.h"
#include "Component.h"
#include "assets/AssetManager.h"
#include "events/EventManager.h"

namespace SNAKE {
	class StaticMeshComponent : public Component {
	public:
		StaticMeshComponent(class Entity* p_entity) : Component(p_entity) {
			mesh_asset = AssetManager::GetAsset<StaticMeshAsset>(AssetManager::SPHERE_MESH);
			materials = mesh_asset->data->materials;
		};

		void SetMeshAsset(AssetRef<StaticMeshAsset> mesh) {
			mesh_asset = mesh;
			EventManagerG::DispatchEvent(ComponentEvent<StaticMeshComponent>(this, ComponentEventType::UPDATED));
		}

		StaticMeshAsset* GetMeshAsset() {
			return mesh_asset.get();
		}

		MaterialAsset* GetMaterial(uint32_t submesh_idx) {
			SNK_DBG_ASSERT(submesh_idx < materials.size());
			return materials[submesh_idx].get();
		}

		const std::vector<AssetRef<MaterialAsset>>& GetMaterials() {
			return materials;
		}

	private:
		AssetRef<StaticMeshAsset> mesh_asset{ nullptr };
		std::vector<AssetRef<MaterialAsset>> materials;

		friend class SceneSerializer;
		friend class EntityEditor;
	};
}