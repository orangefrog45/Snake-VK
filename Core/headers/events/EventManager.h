#pragma once
#include "util/UUID.h"
#include "util/util.h"

namespace SNAKE {
	// p_event argument is a downcasted ptr from actual Event type, dynamic_cast back in the callback if needed
	using listener_callback = std::function<void(std::any _event)>;

	class EventListener {
	public:
		EventListener() = default;
		~EventListener() { if (DeathCallback) DeathCallback(*this); }

		// _event arg will always be a pointer to the event being listened to
		listener_callback callback = nullptr;
	public:
		// Copying only allowed by GlobalEventManager
		EventListener(const EventListener& other) = default;

		// Given by GlobalEventManager during RegisterListener, this erases the cloned listener stored in GlobalEventManager
		void (*DeathCallback)(const EventListener& listener) = nullptr;

		uint16_t m_listening_event_id;
		SNAKE::UUID<uint64_t> m_id;

		friend class EventManagerG;
	};


	class EventManagerG {
	public:
		template<typename EventType>
		static void RegisterListener(EventListener& listener) {
			if (!listener.callback) {
				SNK_CORE_ERROR("Failed to register event listener, no callback provided");
				return;
			}

			auto key = util::GetTypeID<EventType>();
			listener.m_listening_event_id = key;

			m_listeners[key].push_back(listener);
			m_listeners[key][m_listeners[key].size() - 1].m_id = listener.m_id;

			// Only give death callback to original listener, not clone
			listener.DeathCallback = [](const EventListener& listener) {
				DeregisterListener(listener);
				};
		};

		static void DeregisterListener(const EventListener& listener) {
			auto& listener_vec = m_listeners[listener.m_listening_event_id];

			for (size_t i = 0; i < listener_vec.size(); i++) {
				if (listener_vec[i].m_id() == listener.m_id()) {
					listener_vec.erase(listener_vec.begin() + i);
					break;
				}
			};
		}

		template<typename EventType>
		static void DispatchEvent(const EventType& _event) {
			auto key = util::GetTypeID<EventType>();

			if (m_listeners.contains(key)) {
				for (auto& listener : m_listeners[key]) {
					listener.callback(&_event);
				}
			};
		};

	private:
		inline static std::unordered_map<uint16_t, std::vector<EventListener>> m_listeners;
	};

};