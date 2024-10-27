#include "scene/Scene.h"
#include "scene/LightBufferSystem.h"
#include "scene/CameraSystem.h"
#include "scene/SceneInfoBufferSystem.h"
#include "scene/TransformSystem.h"
#include "scene/RaytracingBufferSystem.h"
#include "scene/TransformBufferSystem.h"
#include "scene/SceneSnapshotSystem.h"
#include "scene/ParticleSystem.h"
#include "components/Components.h"

using namespace SNAKE;

void Scene::AddDefaultSystems() {
	RegisterComponent<TransformComponent>();
	RegisterComponent<StaticMeshComponent>();
	RegisterComponent<RaytracingInstanceBufferIdxComponent>();
	RegisterComponent<TransformBufferIdxComponent>();
	RegisterComponent<PointlightComponent>();
	RegisterComponent<SpotlightComponent>();

	AddSystem<TransformSystem>();
	AddSystem<CameraSystem>();
	AddSystem<LightBufferSystem>();
	AddSystem<SceneInfoBufferSystem>();
	AddSystem<RaytracingInstanceBufferSystem>();
	AddSystem<TransformBufferSystem>();
	AddSystem<SceneSnapshotSystem>();
}

Entity* Scene::GetEntity(entt::entity handle) {
	auto* p_tag = m_registry.try_get<TagComponent>(handle);
	return p_tag ? p_tag->GetEntity() : nullptr;
}

Entity* Scene::GetEntity(uint64_t _uuid) {
	return m_uuid_entity_lookup[_uuid];
}

Entity* Scene::GetEntity(const std::string& _name) {
	for (auto [ent, tag] : m_registry.view<TagComponent>().each()) {
		if (tag.name == _name)
			return GetEntity(ent);
	}

	return nullptr;
}
