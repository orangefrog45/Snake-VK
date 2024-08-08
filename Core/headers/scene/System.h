#pragma once

namespace SNAKE {
	class System {
	public:
		virtual ~System() = default;

		virtual void OnSystemAdd() {};
		virtual void OnSceneStart() {};
		virtual void OnUpdate() {};
		virtual void OnSceneEnd() {};
		virtual void OnSceneShutdown() {};

		class Scene* p_scene = nullptr;
	};
}