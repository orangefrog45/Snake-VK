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

	struct FrameStartEvent : public Event {
		FrameStartEvent(uint8_t idx) : frame_flight_index(idx) { };
		uint8_t frame_flight_index;
	};
}