#pragma once
#include "entt/entity/registry.hpp"
#include "components/Component.h"
#include "components/TransformComponent.h"
#include "util/UUID.h"

namespace SNAKE {
	class Entity {
	public:
		Entity(class Scene* p_scene, entt::entity handle, entt::registry* p_reg, uint64_t uuid) : mp_scene(p_scene), mp_registry(p_reg), m_entt_handle(handle), m_uuid(uuid) {};

		template<std::derived_from<Component> T, typename... Args>
		T* AddComponent(Args&&... args) {
			if (GetComponent<T>()) 
				return GetComponent<T>();

			auto* p_comp = &mp_registry->emplace<T>(m_entt_handle, this, std::forward<Args>(args)...);
			EventManagerG::DispatchEvent(ComponentEvent<T>(p_comp, ComponentEventType::ADDED));
			return p_comp;
		}

		template<std::derived_from<Component> T>
		T* GetComponent() {
			return mp_registry->try_get<T>(m_entt_handle);
		}

		template<std::derived_from<Component> T>
		void RemoveComponent() {
			static_assert(!IsFixedComponent<T>(), "Component type cannot be removed from entity, it is a fixed component");

			if (!HasComponent<T>())
				return;

			EventManagerG::DispatchEvent(ComponentEvent<T>(GetComponent<T>, ComponentEventType::REMOVED));
			mp_registry->erase<T>(m_entt_handle);
		}


		// Do not delete children or rearrange them in the scene graph in the callback function
		void ForEachChildRecursive(std::function<void(entt::entity)> func_ptr);

		// Do not delete children or rearrange them in the scene graph in the callback function
		void ForEachLevelOneChild(std::function<void(entt::entity)> func_ptr);

		entt::entity GetParent() {
			return GetComponent<RelationshipComponent>()->parent;
		}

		entt::entity GetEnttHandle() {
			return m_entt_handle;
		}

		Entity* GetChild(const std::string& name) {
			auto& rel_comp = mp_registry->get<RelationshipComponent>(m_entt_handle);
			entt::entity current_entity = rel_comp.first;

			for (int i = 0; i < rel_comp.num_children; i++) {
				auto* p_ent = mp_registry->get<TransformComponent>(current_entity).GetEntity();

				if (p_ent->GetComponent<TagComponent>()->name == name)
					return p_ent;

				current_entity = mp_registry->get<RelationshipComponent>(current_entity).next;
			}

			return nullptr;
		}

		uint64_t GetUUID() const {
			return m_uuid();
		}

		bool HasChildren() {
			return GetComponent<RelationshipComponent>()->first != entt::null;
		}

		entt::registry* GetRegistry() {
			return mp_registry;
		}

		Scene* GetScene() {
			return mp_scene;
		}

		void SetParent(Entity& parent_entity);

		void RemoveParent();
	private:

		void ForEachChildRecursiveInternal(std::function<void(entt::entity)> func_ptr, entt::entity search_entity);

		template<std::derived_from<Component> T>
		constexpr bool IsFixedComponent() {
			return std::is_same_v<T, TransformComponent> | std::is_same_v<T, TagComponent>;
		}

		Scene* mp_scene = nullptr;
		entt::registry* mp_registry = nullptr;
		entt::entity m_entt_handle = entt::null;
		UUID<uint64_t> m_uuid;

		friend class SceneSerializer;
		friend Scene;
	};

	struct EntityDeleteEvent : public Event {
		EntityDeleteEvent(Entity* _ent) : p_ent(_ent) {}
		Entity* p_ent = nullptr;
	};
}