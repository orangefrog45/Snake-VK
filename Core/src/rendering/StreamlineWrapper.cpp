#include "rendering/StreamlineWrapper.h"
#include "core/VkIncl.h"
#include "util/util.h"
#include "Windows.h"
#include "events/EventsCommon.h"

using namespace SNAKE;

static void SL_LogMessageCallback(sl::LogType type, const char* msg) {
	switch (type) {
	case sl::LogType::eInfo:
		SNK_CORE_INFO(msg);
		break;
	case sl::LogType::eWarn:
		SNK_CORE_WARN(msg);
		break;
	case sl::LogType::eError:
		SNK_CORE_ERROR(msg);
		break;
	case sl::LogType::eCount:
		SNK_CORE_TRACE(msg);
		break;
	}

}
bool StreamlineWrapper::Init() {
	auto features = util::array(sl::kFeaturePCL, sl::kFeatureReflex, sl::kFeatureDLSS);
	sl::Preferences pref{};
	pref.showConsole = true;
	pref.logLevel = sl::LogLevel::eVerbose;
	pref.featuresToLoad = features.data();
	pref.numFeaturesToLoad = (uint32_t)features.size();
	pref.logMessageCallback = SL_LogMessageCallback;
	pref.renderAPI = sl::RenderAPI::eVulkan;
	pref.applicationId = 1;

	if (SL_FAILED(res, slInit(pref))) {
		SNK_CORE_ERROR("SlInit failed, err: {}", static_cast<uint32_t>(res));
		return false;
	}

	Get().m_initialized = true;

	return true;
}

void StreamlineWrapper::AcquireNewFrameToken() {
	if (SL_FAILED(res, slGetNewFrameToken(Get().m_current_frame))) {
		SNK_CORE_ERROR("slGetNewFrameToken failed, err: {}", (uint32_t)res);
	}
}


sl::DLSSOptimalSettings StreamlineWrapper::GetDLSS_OptimalSettings(const sl::DLSSOptions& options) {
	sl::DLSSOptimalSettings dlss_settings{};

	if (SL_FAILED(res, slDLSSGetOptimalSettings(options, dlss_settings))) {
		SNK_CORE_ERROR("Failed getting DLSS Optimal settings, err: {}", static_cast<uint32_t>(res));
	}

	return dlss_settings;
}


StreamlineWrapper::InterposerSymbols StreamlineWrapper::LoadInterposer() {
	return Get().LoadInterposerImpl();
}

StreamlineWrapper::InterposerSymbols StreamlineWrapper::LoadInterposerImpl() {
	SNK_ASSERT(!m_interposer.has_value());

	m_interposer = LoadLibrary("sl.interposer.dll");
	SNK_ASSERT(std::any_cast<HMODULE>(m_interposer));

	StreamlineWrapper::InterposerSymbols symbols;
	symbols.get_instance_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(std::any_cast<HMODULE>(m_interposer), "vkGetInstanceProcAddr"));
	SNK_ASSERT(symbols.get_instance_proc_addr);

	symbols.create_device = reinterpret_cast<PFN_vkCreateDevice>(GetProcAddress(std::any_cast<HMODULE>(m_interposer), "vkCreateDevice"));
	SNK_ASSERT(symbols.create_device);

	symbols.create_instance = reinterpret_cast<PFN_vkCreateInstance>(GetProcAddress(std::any_cast<HMODULE>(m_interposer), "vkCreateInstance"));
	SNK_ASSERT(symbols.create_instance);

	return symbols;
}

void StreamlineWrapper::Shutdown() {
	if (SL_FAILED(res, slShutdown())) {
		SNK_CORE_ERROR("Streamline failed to shutdown, err: {}", static_cast<uint32_t>(res));
	}

	FreeLibrary(std::any_cast<HMODULE>(Get().m_interposer));
}

sl::float4x4 StreamlineWrapper::GlmToSl(const glm::mat4 m) {
	sl::float4x4 res;
	res.row[0].x = m[0][0];
	res.row[0].y = m[1][0];
	res.row[0].z = m[2][0];
	res.row[0].w = m[3][0];
	res.row[1].x = m[0][1];
	res.row[1].y = m[1][1];
	res.row[1].z = m[2][1];
	res.row[1].w = m[3][1];
	res.row[2].x = m[0][2];
	res.row[2].y = m[1][2];
	res.row[2].z = m[2][2];
	res.row[2].w = m[3][2];
	res.row[3].x = m[0][3];
	res.row[3].y = m[1][3];
	res.row[3].z = m[2][3];
	res.row[3].w = m[3][3];
	return res;
}
