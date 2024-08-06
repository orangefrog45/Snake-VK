#pragma once
#include "entt/entity/registry.hpp"

namespace SNAKE {
	class Component {
	public:
		Component(class Entity* _entity) : p_entity(_entity) {};

		Entity* GetEntity() { return p_entity; }
	private:
		Entity* p_entity = nullptr;
	};
}