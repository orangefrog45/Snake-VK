cmake_minimum_required(VERSION 3.8)
set (CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(SNAKE_VK_CORE)


set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd /MP /bigobj" CACHE INTERNAL "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD /MP /O2 /j12" CACHE INTERNAL "" FORCE)

add_subdirectory("Core")
add_subdirectory("Editor")

if(MSVC)
    target_compile_options(SNAKE_VK_CORE PRIVATE /W4)
endif()