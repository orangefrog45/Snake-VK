#pragma once
#include "core/Window.h"
#include "layers/LayerManager.h"


namespace SNAKE {
	class App {
	public:
		~App();

		void Init(const char* name);
		void MainLoop();

		Window window;
		LayerManager layers{ &window };
	private:
		vk::UniqueDebugUtilsMessengerEXT m_messenger;

		void CreateDebugCallback();
	};
}