cmake_minimum_required(VERSION 3.8)

project(ORNG_VK_CORE)

add_executable(ORNG_VK_CORE
src/main.cpp
)

target_link_directories(ORNG_VK_CORE PUBLIC $ENV{VULKAN_SDK}/Lib)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
set(VK_LIBRARIES 
"SPIRVd.lib"
"SPIRV-Toolsd.lib"
"SPIRV-Tools-diffd.lib"
"SPIRV-Tools-optd.lib"
"OGLCompilerd.lib"
"OSDependentd.lib"
"GenericCodeGend.lib"
"MachineIndependentd.lib"
)
else()
set(VK_LIBRARIES 
"SPIRV.lib"
"SPIRV-Tools.lib"
"SPIRV-Tools-diff.lib"
"SPIRV-Tools-opt.lib"
"OGLCompiler.lib"
"OSDependent.lib"
"GenericCodeGen.lib"
"MachineIndependent.lib"
)
endif()

target_link_libraries(ORNG_VK_CORE PUBLIC
$ENV{VULKAN_SDK}/Lib/vulkan-1
VK_LIBRARIES
)

target_include_directories(ORNG_VK_CORE PUBLIC
$ENV{VULKAN_SDK}/Include
headers
)