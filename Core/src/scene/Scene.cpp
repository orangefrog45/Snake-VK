#include "scene/Scene.h"
#include "scene/LightBufferSystem.h"
#include "scene/CameraSystem.h"
#include "scene/SceneInfoBufferSystem.h"

using namespace SNAKE;

void Scene::AddDefaultSystems() {
	AddSystem<CameraSystem>();
	AddSystem<LightBufferSystem>();
	AddSystem<SceneInfoBufferSystem>();
}
