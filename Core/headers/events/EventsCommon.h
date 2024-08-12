#pragma once
#include "EventManager.h"

namespace SNAKE {
	struct Event {
		virtual ~Event() = default;
	};

	struct WindowEvent : public Event {
		enum class Type {
			RESIZE
		} type;

		uint32_t prev_width = 0;
		uint32_t prev_height = 0;

		uint32_t new_width = 0;
		uint32_t new_height = 0;
	};

	struct FrameSyncFenceEvent : public Event {
	};

	struct FrameStartEvent : public Event {
	};
	
	struct EngineUpdateEvent : public Event {

	};

	struct EngineRenderEvent : public Event {

	};

	struct EngineShutdownEvent : public Event {

	};

}