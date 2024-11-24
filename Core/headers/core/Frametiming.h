#pragma once

namespace ScriptInterface {
	class FrameTiming;
}

namespace SNAKE {

	class FrameTiming {
		friend class ScriptInterface::FrameTiming;
	public:
		static FrameTiming& Get() {
			static FrameTiming s_instance;
			return s_instance;
		}

		static float GetTimeStep() {
			return Get().IGetTimeStep();
		}

		static float GetTotalElapsedTime() {
			return Get().total_elapsed_time;
		}

		static unsigned GetFrameCount() {
			return Get().current_frame;
		}

		static void Update() {
			Get().IUpdate();
		}


	private:

		void IUpdate() {
			current_frame++;

			last_frame_time = current_frame_time;
			current_frame_time = std::chrono::steady_clock::now();
			current_frame_time_step = std::chrono::duration_cast<std::chrono::microseconds>(current_frame_time - last_frame_time).count() / 1000.f;
			total_elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - m_application_start_time).count() / 1000.f;
		}

		float IGetTimeStep() const {
			return current_frame_time_step;
		}

		unsigned current_frame = 0;
		float current_frame_time_step = 0;
		float total_elapsed_time = 0;
		std::chrono::steady_clock::time_point last_frame_time = std::chrono::steady_clock::now();
		std::chrono::steady_clock::time_point current_frame_time = std::chrono::steady_clock::now();

		inline const static std::chrono::steady_clock::time_point m_application_start_time = std::chrono::steady_clock::now();

	};
}