cmake_minimum_required(VERSION 3.8)

project(SNAKE_VK_CORE)


find_package(Vulkan REQUIRED)
find_library(SHADERC_LIB shaderc_combined $ENV{VULKAN_SDK}/Lib)
add_subdirectory(extern/vma SYSTEM)

add_subdirectory(extern/glfw)
target_compile_options(glfw PRIVATE /MD$<$<CONFIG:Debug>:d>)

add_subdirectory(extern/spdlog)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX ON CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/extern/assimp)


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
 "src/events/EventManager.cpp" 
 "headers/util/UUID.h" 
 "headers/util/FileUtil.h" 
 "src/util/UUID.cpp" 
 "src/core/Window.cpp" 
 "headers/events/EventsCommon.h" 
 "headers/core/VulkanApp.h" 
 "headers/core/VkCommon.h" 
 "src/core/VkCommon.cpp" 
 "headers/core/VkContext.h" 
 "src/core/VkContext.cpp" 
 "headers/core/VkIncl.h" 
 "src/misc/stb.cpp" 
 "headers/textures/Textures.h" 
 "src/textures/Textures.cpp" 
 "src/misc/vma.cpp"
 "src/assets/MeshData.cpp" "headers/assets/MeshData.h" "headers/core/S_VkBuffer.h" "src/core/S_VkBuffer.cpp" "headers/core/DescriptorBuffer.h" "headers/core/Pipelines.h" "headers/shaders/ShaderLibrary.h" "src/core/Pipelines.cpp")


target_link_options(SNAKE_VK_CORE PRIVATE -INCREMENTAL -ZI)
target_compile_options(SNAKE_VK_CORE PRIVATE -ZI)
target_link_directories(SNAKE_VK_CORE PUBLIC $ENV{VULKAN_SDK}/Lib)
target_precompile_headers(SNAKE_VK_CORE PUBLIC
headers/pch/pch.h
)

target_link_libraries(SNAKE_VK_CORE PUBLIC
${Vulkan_LIBRARIES}
${SHADERC_LIB}
GPUOpen::VulkanMemoryAllocator
spdlog::spdlog
glfw
assimp
)

target_include_directories(SNAKE_VK_CORE PUBLIC
${Vulkan_INCLUDE_DIRS}
headers
extern/spdlog/include
extern/glfw/include
extern/glm
extern/stb
extern/vma/include
extern/assimp/include
)



