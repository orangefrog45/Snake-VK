#pragma once

#include "assets/MeshData.h"
#include "Component.h"
#include "assets/AssetManager.h"

namespace SNAKE {
	class MeshComponent : public Component {
	public:
		MeshComponent(class Entity* p_entity) : Component(p_entity) {
			mesh_asset = AssetManager::GetAsset<StaticMeshAsset>(AssetManager::SPHERE_MESH);
		};

		AssetRef<StaticMeshAsset> mesh_asset{ nullptr };
	};
}