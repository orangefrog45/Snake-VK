#pragma once
#include "entt/entity/registry.hpp"
#include "events/EventsCommon.h"

namespace SNAKE {
	class Component {
	public:
		Component(class Entity* _entity) : p_entity(_entity) {};

		Entity* GetEntity() { return p_entity; }
	private:
		Entity* p_entity = nullptr;
	};

	enum class ComponentEventType : uint8_t {
		ADDED,
		UPDATED,
		REMOVED
	};

	template<std::derived_from<Component> T>
	struct ComponentEvent : public Event {
		ComponentEvent() = delete;
		ComponentEvent(T* comp, ComponentEventType type) : p_component(comp), event_type(type) {};

		T* p_component;
		std::byte* p_data = nullptr;
		
		ComponentEventType event_type;
		uint8_t event_code = 0;
	};
}