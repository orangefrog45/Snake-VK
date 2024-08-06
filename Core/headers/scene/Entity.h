#pragma once
#include "entt/entity/registry.hpp"
#include "components/Component.h"
#include "util/UUID.h"

namespace SNAKE {
	class Entity {
	public:
		Entity(class Scene* p_scene, entt::entity handle, entt::registry* p_reg, uint64_t uuid) : mp_scene(p_scene), m_entt_handle(handle), mp_registry(p_reg), m_uuid(uuid) {};

		template<std::derived_from<Component> T, typename... Args>
		T* AddComponent(Args&&... args) {
			if (GetComponent<T>()) 
				return GetComponent<T>();

			return &mp_registry->emplace<T>(m_entt_handle, this, std::forward<Args>(args)...);
		}

		template<std::derived_from<Component> T>
		T* GetComponent() {
			return mp_registry->try_get<T>(m_entt_handle);
		}

		template<std::derived_from<Component> T>
		void RemoveComponent() {
			if (!HasComponent<T>())
				return;

			mp_registry->erase<T>(m_entt_handle);
		}


	private:
		Scene* mp_scene = nullptr;
		entt::registry* mp_registry = nullptr;
		entt::entity m_entt_handle = entt::null;
		UUID<uint64_t> m_uuid;

		friend Scene;
	};
}