#include "core/App.h"
#include "EditorLayer.h"

using namespace SNAKE;

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

int main() {
	App snake_app;
	EditorLayer* p_editor = new EditorLayer{ &snake_app.window };

	snake_app.layers.PushLayer(p_editor);
	snake_app.Init("Snake app");
	snake_app.MainLoop();

	return 0;
}