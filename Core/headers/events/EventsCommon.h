#include "EventManager.h"

namespace SNAKE {
	struct WindowEvent {
		enum class Type {
			RESIZE
		} type;

		uint32_t prev_width = 0;
		uint32_t prev_height = 0;

		uint32_t new_width = 0;
		uint32_t new_height = 0;
	};
}