#pragma once

#include "Logger.h"
#include <tuple>

#ifdef PRODUCTION_RELEASE
#define SNK_BREAK(...) {SNK_CORE_CRITICAL(__VA_ARGS__); SNAKE::Logger::Flush(); exit(1);}
#define SNK_DEBUG_BREAK(...)
#else
#define SNK_BREAK(...) {SNK_CORE_ERROR(__VA_ARGS__); SNAKE::Logger::Flush(); __debugbreak();}
#define SNK_DEBUG_BREAK(...) {SNK_CORE_ERROR(__VA_ARGS__); SNAKE::Logger::Flush(); __debugbreak();}
#endif

#define SNK_CHECK_VK_RESULT_MSG(res, msg) \
	if (res != vk::Result::eSuccess) \
		{SNK_BREAK("Error in '{0}', code: '{1}', message: '{2}'", #res, (int)res, msg);}

#define SNK_CHECK_VK_RESULT(res) \
	if (static_cast<vk::Result>(res) != vk::Result::eSuccess) \
		{SNK_BREAK("Error in '{0}', code: '{1}'", #res, (int)res);}

#ifndef PRODUCTION_RELEASE
#define SNK_DBG_CHECK_VK_RESULT(res, msg) \
	if (res != vk::Result::eSuccess) \
		{SNK_BREAK("Error in '{0}', code: '{1}'", msg, (int)res);}
#else
#define SNK_DBG_CHECK_VK_RESULT(res, msg) ;
#endif

#define SNK_ASSERT(x) if (!(x)) {SNK_BREAK("Assertion failed");}
#define SNK_ASSERT_ARG(x, ...) if (!(x)) {SNK_BREAK("Assertion failed: '{0}'", __VA_ARGS__);}

namespace SNAKE {
	namespace util {
		template<typename Container, typename... Args>
			requires((std::is_convertible_v<Args, typename Container::value_type>, ...))
		void PushBackMultiple(Container& container, Args&&... args) {
			(container.push_back(std::forward<Args>(args)), ...);
		};
		
		template<typename ArrayType, typename ...Args>
		constexpr std::array<typename std::decay<ArrayType>::type, sizeof...(Args) + 1> array(ArrayType&& first, Args&&... args) {
			return { { std::forward<ArrayType>(first),  std::forward<Args>(args)... } };
		}



		/* Type ID Stuff */
		inline uint16_t type_id_seq = 0;
		template< typename T > inline const uint16_t type_id = type_id_seq++;

		template<typename T>
		uint16_t GetTypeID() {
			return type_id<T>;
		}
		/*----------------*/



	}

}
