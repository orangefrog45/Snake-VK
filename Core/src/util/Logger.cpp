#include "pch/pch.h"
#include <fstream>
#include "spdlog/sinks/ringbuffer_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

#include "Logger.h"


namespace SNAKE {
	// How many logged messages are saved and stored in memory that can be retrieved by the program
	constexpr int MAX_LOG_HISTORY = 20;

	static auto s_ringbuffer_sink = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(MAX_LOG_HISTORY);
	static auto s_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt");
	std::shared_ptr<spdlog::logger> Logger::s_core_logger;


	void Logger::Init() {
		// Clear log file
		std::ofstream("log.txt", std::ios::trunc);

		std::vector<spdlog::sink_ptr> sinks;
		s_ringbuffer_sink->set_pattern("% %v%$");
		sinks.push_back(s_ringbuffer_sink);
		sinks.push_back(s_file_sink);

		auto color_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		color_sink->set_pattern("%^[%T] %n: %v%$");
		sinks.push_back(color_sink);

		s_core_logger = std::make_shared<spdlog::logger>("SNAKE", sinks.begin(), sinks.end());
		s_core_logger->set_level(spdlog::level::trace);
	}

	std::string Logger::GetLastLog() {
		return s_ringbuffer_sink->last_formatted()[4];
	}

	std::vector<std::string> Logger::GetLastLogs() {
		return std::move(s_ringbuffer_sink->last_formatted());
	}

	void Logger::Flush() {
		GetCoreLogger()->flush();
	}


	std::shared_ptr<spdlog::logger>& Logger::GetCoreLogger() {
		return s_core_logger;
	}


}