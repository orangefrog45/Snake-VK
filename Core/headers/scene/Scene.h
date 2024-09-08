#pragma once
#include "Entity.h"
#include "components/TransformComponent.h"
#include "System.h"
#include "components/Lights.h"

namespace SNAKE {
	class Scene {
	public:
		Scene() = default;
		~Scene() { 
			ClearEntities();
			for (auto [_uuid, system] : m_systems) {
				delete system;
			}
			m_systems.clear();
		};

		entt::registry& GetRegistry() {
			return m_registry;
		}

		Entity& CreateEntity(uint64_t _uuid = 0) {
			// Randomize uuid if left as default
			if (_uuid == 0)
				_uuid = UUID<uint64_t>()();

			SNK_ASSERT(!m_uuid_entity_lookup.contains(_uuid));

			Entity* p_ent = new Entity(this, m_registry.create(), &m_registry, _uuid);
			p_ent->AddComponent<TransformComponent>();
			p_ent->AddComponent<TagComponent>();
			p_ent->AddComponent<RelationshipComponent>();

			m_entities.push_back(p_ent);
			m_uuid_entity_lookup[p_ent->m_uuid()] = p_ent;
			return *p_ent;
		}

		template<std::derived_from<System> T>
		T* GetSystem() {
			if (!m_systems.contains(util::type_id<T>)) {
				SNK_CORE_ERROR("Scene::GetSystem failed, no valid system found");
				return nullptr;
			}

			return dynamic_cast<T*>(m_systems[util::type_id<T>]);
		}

		template<std::derived_from<System> T, typename... Args>
		T* AddSystem(Args&&... args) {
			if (m_systems.contains(util::type_id<T>)) {
				SNK_CORE_ERROR("Failed to add system to scene, duplicate detected");
				return GetSystem<T>();
			}
				
			auto* p_sys = new T(std::forward<Args>(args)...);
			p_sys->p_scene = this;
			p_sys->OnSystemAdd();
			m_systems[util::type_id<T>] = p_sys;
			return p_sys;
		}

		void Update() {
			for (auto [id, p_system] : m_systems) {
				p_system->OnUpdate();
			}
		}

		void ClearEntities() {
			while (!m_entities.empty()) {
				DeleteEntity(m_entities[0]);
			}
		}

		const std::vector<Entity*>& GetEntities() {
			return m_entities;
		}

		Entity* GetEntity(entt::entity handle);

		Entity* GetEntity(uint64_t uuid);

		void DeleteEntity(Entity* p_ent) {
			EventManagerG::DispatchEvent(EntityDeleteEvent(p_ent));

			m_registry.destroy(p_ent->m_entt_handle);
			auto it = std::ranges::find(m_entities, p_ent);
			SNK_ASSERT(it != m_entities.end());
			m_entities.erase(it);
			m_uuid_entity_lookup.erase(p_ent->m_uuid());

			delete p_ent;
		}

		void AddDefaultSystems();

		std::string name = "Unnamed scene";
		UUID<uint64_t> uuid;

		DirectionalLight directional_light;
	private:
		entt::registry m_registry;

		std::vector<Entity*> m_entities;

		std::unordered_map<uint64_t, Entity*> m_uuid_entity_lookup;

		std::unordered_map<util::TypeID, System*> m_systems;

		friend class SceneSerializer;
	};
}