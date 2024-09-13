#include "scene/Scene.h"
#include "scene/LightBufferSystem.h"
#include "scene/CameraSystem.h"
#include "scene/SceneInfoBufferSystem.h"
#include "scene/TransformSystem.h"
#include "scene/RaytracingBufferSystem.h"
#include "scene/TransformBufferSystem.h"
#include "scene/SceneSnapshotSystem.h"

using namespace SNAKE;

void Scene::AddDefaultSystems() {
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
