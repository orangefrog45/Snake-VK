#pragma once
#include "core/VkIncl.h"
#include "events/EventManager.h"

#include <sl.h>
#include <sl_consts.h>
#include <sl_dlss.h>

namespace SNAKE {
	class StreamlineWrapper {
		struct InterposerSymbols {
			PFN_vkGetInstanceProcAddr get_instance_proc_addr;
			PFN_vkCreateDevice create_device;
			PFN_vkCreateInstance create_instance;
		};
	public:
		static StreamlineWrapper& Get() {
			static StreamlineWrapper s_instance;
			return s_instance;
		}

		// Return value indicates if streamline is available or not
		static bool Init();
		
		static InterposerSymbols LoadInterposer();

		static void Shutdown();

		static sl::DLSSOptimalSettings GetDLSS_OptimalSettings(const sl::DLSSOptions& options);

		static void AcquireNewFrameToken();

		static sl::FrameToken* GetCurrentFrameToken() {
			return Get().m_current_frame;
		}

		static sl::float4x4 GlmToSl(const glm::mat4 m);

		static bool IsAvailable() {
			return Get().m_initialized;
		}

	private:
		InterposerSymbols LoadInterposerImpl();

		bool m_initialized = false;

		sl::FrameToken* m_current_frame = nullptr;

		// HMODULE, declaring as std::any so windows header doesn't have to be included here
		std::any m_interposer;
	};
}