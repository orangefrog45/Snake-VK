#include "Entity.h"
#include "components/TransformComponent.h"

namespace SNAKE {
	class Scene {
	public:
		Scene() = default;
		~Scene() { Clear(); };

		entt::registry& GetRegistry() {
			return m_registry;
		}

		Entity* CreateEntity() {
			Entity* p_ent = new Entity(this, m_registry.create(), &m_registry, UUID<uint64_t>()());
			p_ent->AddComponent<TransformComponent>();

			m_entities.push_back(p_ent);
			return p_ent;
		}

		void Clear() {
			while (!m_entities.empty()) {
				DeleteEntity(m_entities[0]);
			}
		}

		void DeleteEntity(Entity* p_ent) {
			m_registry.destroy(p_ent->m_entt_handle);
			auto it = std::ranges::find(m_entities, p_ent);
			SNK_ASSERT(it != m_entities.end());
			m_entities.erase(it);

			delete p_ent;
		}


	private:
		entt::registry m_registry;

		std::vector<Entity*> m_entities;
	};
}