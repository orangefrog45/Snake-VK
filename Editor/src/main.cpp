#include "core/App.h"
#include "EditorLayer.h"

using namespace SNAKE;

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

void main() {
	App snake_app;
	EditorLayer editor{ &snake_app.window };

	snake_app.layers.PushLayer(&editor);
	snake_app.Init("Snake app");
	snake_app.MainLoop();
}