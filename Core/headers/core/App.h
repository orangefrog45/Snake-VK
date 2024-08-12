#pragma once
#include "core/Window.h"
#include "LayerManager.h"


namespace SNAKE {
	class App {
	public:
		~App();

		void Init(const char* name);
		void MainLoop();

		Window window;
		LayerManager layers;
	private:
		vk::UniqueDebugUtilsMessengerEXT m_messenger;
		void CreateDebugCallback();
	};
}