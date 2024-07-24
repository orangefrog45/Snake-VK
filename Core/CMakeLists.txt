cmake_minimum_required(VERSION 3.8)

project(SNAKE_VK_CORE)

add_executable(SNAKE_VK_CORE
src/main.cpp
"headers/util/util.h"
"headers/util/Logger.h"
"src/util/util.cpp"
"src/util/Logger.cpp"
"src/pch/pch.cpp"
"headers/pch/pch.h"


"headers/core/Window.h"
"headers/events/EventManager.h"
 "src/events/EventManager.cpp" "headers/util/UUID.h" "headers/util/FileUtil.h" "src/util/UUID.cpp" "src/core/Window.cpp" "headers/events/EventsCommon.h" "headers/core/VulkanApp.h" "headers/core/VkCommon.h" "src/core/VkCommon.cpp" "headers/core/VkContext.h" "src/core/VkContext.cpp" "headers/core/VkIncl.h" "src/misc/stb.cpp" "headers/textures/Textures.h" "src/textures/Textures.cpp")

target_link_options(SNAKE_VK_CORE PRIVATE -INCREMENTAL -ZI)
target_compile_options(SNAKE_VK_CORE PRIVATE -ZI)
target_link_directories(SNAKE_VK_CORE PUBLIC $ENV{VULKAN_SDK}/Lib)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
set(VK_LIBRARIES
"vulkan-1.lib" 
"SPIRVd.lib"
"SPIRV-Toolsd.lib"
"SPIRV-Tools-diffd.lib"
"SPIRV-Tools-optd.lib"
"OSDependentd.lib"
"GenericCodeGend.lib"
"MachineIndependentd.lib"
)
else()
set(VK_LIBRARIES
"vulkan-1.lib" 
"SPIRV.lib"
"SPIRV-Tools.lib"
"SPIRV-Tools-diff.lib"
"SPIRV-Tools-opt.lib"
"OSDependent.lib"
"GenericCodeGen.lib"
"MachineIndependent.lib"
)
endif()

find_package(Vulkan REQUIRED)
find_library(SHADERC_LIB shaderc_combined $ENV{VULKAN_SDK}/Lib)

target_link_libraries(SNAKE_VK_CORE PUBLIC
${Vulkan_LIBRARIES}
${SHADERC_LIB}
spdlog::spdlog
glfw
)

target_include_directories(SNAKE_VK_CORE PUBLIC
${Vulkan_INCLUDE_DIRS}
headers
extern/spdlog/include
extern/glfw/include
extern/glm
extern/stb
)

target_precompile_headers(SNAKE_VK_CORE PUBLIC
headers/pch/pch.h
)

add_subdirectory(extern/glfw)
target_compile_options(glfw PRIVATE /MD$<$<CONFIG:Debug>:d>)

add_subdirectory(extern/spdlog)
