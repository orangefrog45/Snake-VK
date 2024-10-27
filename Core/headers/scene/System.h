#pragma once

namespace SNAKE {
	class System {
	public:
		virtual ~System() = default;

		virtual void OnSystemAdd() {};
		virtual void OnSceneStart() {};
		virtual void OnUpdate() {};
		virtual void OnSceneEnd() {};
		virtual void OnSystemRemove() {};

		class Scene* p_scene = nullptr;
	};
}