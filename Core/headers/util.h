#pragma once

#include "Logger.h"

#ifdef PRODUCTION_RELEASE
#define SNK_BREAK(...) {SNK_CORE_CRITICAL(__VA_ARGS__); SNAKE::Logger::Flush(); exit(1);}
#define SNK_DEBUG_BREAK(...)
#else
#define SNK_BREAK(...) {SNK_CORE_ERROR(__VA_ARGS__); SNAKE::Logger::Flush(); __debugbreak();}
#define SNK_DEBUG_BREAK(...) {SNK_CORE_ERROR(__VA_ARGS__); SNAKE::Logger::Flush(); __debugbreak();}
#endif

#define SNK_CHECK_VK_RESULT(res, msg) \
	if (res != vk::Result::eSuccess) \
		{SNK_BREAK("Error in '{0}', code: '{1}'", msg, (int)res);}

#ifndef PRODUCTION_RELEASE
#define SNK_DBG_CHECK_VK_RESULT(res, msg) \
	if (res != vk::Result::eSuccess) \
		{SNK_BREAK("Error in '{0}', code: '{1}'", msg, (int)res);}
#else
#define SNK_DBG_CHECK_VK_RESULT(res, msg) ;
#endif

#define SNK_ASSERT(x, ...) if (!(x)) {SNK_BREAK("Assertion failed: '{0}'", __VA_ARGS__);}
