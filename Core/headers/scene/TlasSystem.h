#pragma once
#include "System.h"
#include "resources/TLAS.h"
#include "core/VkCommands.h"

namespace SNAKE {
	// System responsible for building and updating a TLAS for the scene it is attached to
	class TlasSystem : public System {
	public:
		void OnSystemAdd() override;

		void OnSystemRemove() override;

		const TLAS& GetTlas(FrameInFlightIndex idx) {
			return m_tlas_array[idx];
		}

	private:
		void BuildTlas(FrameInFlightIndex idx);

		EventListener m_frame_start_listener;

		std::array<TLAS, MAX_FRAMES_IN_FLIGHT> m_tlas_array;
		std::array<CommandBuffer, MAX_FRAMES_IN_FLIGHT> m_cmd_buffers;
	};

}