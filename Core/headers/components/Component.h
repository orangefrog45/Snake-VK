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

	struct TagComponent : public Component {
		TagComponent(class Entity* p_ent) : Component(p_ent) {}

		std::string name = "New entity";
	};

	struct RelationshipComponent : public Component {
		RelationshipComponent(Entity* p_entity) : Component(p_entity) {};
		size_t num_children = 0;
		entt::entity first{ entt::null };
		entt::entity prev{ entt::null };
		entt::entity next{ entt::null };
		entt::entity parent{ entt::null };
	};

}