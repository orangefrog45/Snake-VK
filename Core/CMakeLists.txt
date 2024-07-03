cmake_minimum_required(VERSION 3.8)

project(ORNG_VK_CORE)

add_executable(ORNG_VK_CORE
src/main.cpp
 "headers/util.h" "headers/Logger.h" "src/util/util.cpp" "src/util/Logger.cpp" "src/pch/pch.cpp" "headers/pch/pch.h")

target_link_directories(ORNG_VK_CORE PUBLIC $ENV{VULKAN_SDK}/Lib)

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

target_link_libraries(ORNG_VK_CORE PUBLIC
${VK_LIBRARIES}
spdlog::spdlog
glfw
)

target_include_directories(ORNG_VK_CORE PUBLIC
$ENV{VULKAN_SDK}/Include
headers
extern/spdlog/include
extern/glfw/include
)

target_precompile_headers(ORNG_VK_CORE PUBLIC
headers/pch/pch.h
)

add_subdirectory(extern/glfw)
target_compile_options(glfw PRIVATE /MD$<$<CONFIG:Debug>:d>)

add_subdirectory(extern/spdlog)
