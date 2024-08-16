#pragma once

#include "assets/MeshData.h"
#include "Component.h"
#include "assets/AssetManager.h"

namespace SNAKE {
	class StaticMeshComponent : public Component {
	public:
		StaticMeshComponent(class Entity* p_entity) : Component(p_entity) {
			mesh_asset = AssetManager::GetAsset<StaticMeshAsset>(AssetManager::SPHERE_MESH);
		};

		AssetRef<StaticMeshAsset> mesh_asset{ nullptr };
	};
}