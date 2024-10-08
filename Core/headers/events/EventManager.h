#pragma once
#include "util/UUID.h"
#include "util/util.h"

namespace SNAKE {
	using listener_callback = std::function<void(struct Event const* _event)>;

	class EventListener {
	public:
		EventListener() = default;
		~EventListener() { if (DeathCallback) DeathCallback(*this); }
		EventListener(const EventListener& other) : callback(other.callback), m_listening_event_id(other.m_listening_event_id) {};

		// _event arg will always be a downcasted pointer to the event being listened to
		listener_callback callback = nullptr;
	private:

		// Given by EventManagerG during RegisterListener, this erases the cloned listener stored in EventManagerG
		void (*DeathCallback)(const EventListener& listener) = nullptr;

		// Given by EventManagerG, unique type-id for event
		uint16_t m_listening_event_id = std::numeric_limits<uint16_t>::max();

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