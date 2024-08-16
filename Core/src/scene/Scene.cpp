#include "scene/Scene.h"
#include "scene/LightBufferSystem.h"
#include "scene/CameraSystem.h"
#include "scene/SceneInfoBufferSystem.h"
#include "scene/TransformSystem.h"

using namespace SNAKE;

void Scene::AddDefaultSystems() {
	AddSystem<TransformSystem>();
	AddSystem<CameraSystem>();
	AddSystem<LightBufferSystem>();
	AddSystem<SceneInfoBufferSystem>();
}

Entity* Scene::GetEntity(entt::entity handle) {
	auto* p_tag = m_registry.try_get<TagComponent>(handle);
	return p_tag ? p_tag->GetEntity() : nullptr;
}
