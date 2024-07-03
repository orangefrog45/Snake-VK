#pragma once

#include "Logger.h"

#ifdef PRODUCTION_RELEASE
#define SNK_BREAK(...) SNK_CORE_CRITICAL(__VA_ARGS__); SNAKE::Logger::Flush(); exit(1)
#else
#define SNK_BREAK(...) SNK_CORE_ERROR(__VA_ARGS__); SNAKE::Logger::Flush(); __debugbreak()

#endif
#define SNK_DEBUG_BREAK(...) SNK_CORE_ERROR(__VA_ARGS__); SNAKE::Logger::Flush(); __debugbreak()
