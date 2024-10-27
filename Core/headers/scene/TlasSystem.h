#pragma once
#include "System.h"
#include "resources/TLAS.h"

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
		EventListener m_frame_start_listener;

		void BuildTlas(FrameInFlightIndex idx);

		std::array<TLAS, MAX_FRAMES_IN_FLIGHT> m_tlas_array;
	};

}