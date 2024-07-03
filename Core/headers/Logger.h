#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace SNAKE {
	class Logger {
	public:
		static void Init();
		static void Flush();
		static std::string GetLastLog();
		static std::vector<std::string> GetLastLogs();
		static std::shared_ptr<spdlog::logger>& GetCoreLogger();
	private:
		static std::shared_ptr<spdlog::logger> s_core_logger;
	};
}

#define SNK_CORE_TRACE(...) SNAKE::Logger::GetCoreLogger()->trace(__VA_ARGS__)
#define SNK_CORE_INFO(...) SNAKE::Logger::GetCoreLogger()->info(__VA_ARGS__)
#define SNK_CORE_WARN(...) SNAKE::Logger::GetCoreLogger()->warn(__VA_ARGS__)
#define SNK_CORE_ERROR(...) SNAKE::Logger::GetCoreLogger()->error(__VA_ARGS__)
#define SNK_CORE_CRITICAL(...) SNAKE::Logger::GetCoreLogger()->critical(__VA_ARGS__)