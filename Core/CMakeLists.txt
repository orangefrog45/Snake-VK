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
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/extern/assimp)

add_subdirectory(extern/json)

add_subdirectory(extern/spirv-cross)

# IMGUI --------------------------------------
add_library(imgui
    extern/imgui/imgui.cpp
    extern/imgui/imgui_demo.cpp
    extern/imgui/imgui_draw.cpp
    extern/imgui/backends/imgui_impl_glfw.cpp
    extern/imgui/backends/imgui_impl_vulkan.cpp
    extern/imgui/misc/cpp/imgui_stdlib.cpp
    extern/imgui/imgui_tables.cpp
    extern/imgui/imgui_widgets.cpp
)
target_include_directories(imgui PUBLIC 
extern/imgui
${Vulkan_INCLUDE_DIRS}
extern/glfw/include
)
# ------------------------------------------

add_library(SNAKE_VK_CORE

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
  
 "headers/core/VkCommon.h" 
 "src/core/VkCommon.cpp" 
 "headers/core/VkContext.h" 
 "src/core/VkContext.cpp" 
 "headers/core/VkIncl.h" 
 "src/misc/stb.cpp" 
 "headers/resources/Images.h" 
 "src/resources/Images.cpp" 
 "src/misc/vma.cpp"
 "src/assets/MeshData.cpp" 
 "headers/assets/MeshData.h" 
 "headers/resources/S_VkBuffer.h" 
 "src/resources/S_VkBuffer.cpp" 
 "headers/core/DescriptorBuffer.h" 
 "headers/core/Pipelines.h" 
 "headers/shaders/ShaderLibrary.h" 
 "src/core/Pipelines.cpp" 
 "headers/core/VkCommands.h"
 "headers/assets/Asset.h"
 "headers/assets/AssetManager.h" 
 "src/assets/AssetManager.cpp" 
 "headers/assets/TextureAssets.h" 
 "headers/scene/Scene.h" 
 "headers/scene/Entity.h" 
 "headers/components/Component.h"
 "headers/components/TransformComponent.h" 
 "src/components/TransformComponent.cpp" 
 "src/util/ExtraMath.cpp" 
 "headers/util/ExtraMath.h" 
 "headers/core/Input.h" 
 "src/core/Input.cpp"  
 "headers/scene/System.h" 
 "headers/scene/LightBufferSystem.h" 
 "headers/components/Lights.h"  
 "src/scene/LightBufferSystem.cpp" 
 "src/scene/SceneInfoBufferSystem.cpp"
 "headers/scene/CameraSystem.h" 
 "headers/components/CameraComponent.h" 
 "src/scene/CameraSystem.cpp" 
 "headers/renderpasses/ShadowPass.h" 
 "src/renderpasses/ShadowPass.cpp" 
 "headers/renderpasses/ForwardPass.h" 
 "src/renderpasses/ForwardPass.cpp" 
 "headers/components/MeshComponent.h" 
 "headers/layers/LayerManager.h" 
 "headers/core/App.h" 
 "headers/rendering/VkRenderer.h" 
 "headers/rendering/VkSceneRenderer.h" 
 "headers/assets/ResourceBufferManagers.h" 
 "src/assets/ResourceBufferManagers.cpp"
 "headers/assets/MaterialAsset.h" 
 "src/core/DescriptorBuffer.cpp" 
 "src/core/App.cpp"
 "src/scene/Scene.cpp" "headers/components/Components.h" "src/rendering/VkRenderer.cpp" "headers/layers/ImGuiLayer.h" "src/layers/LayerManager.cpp" "src/layers/ImGuiLayer.cpp" "src/scene/Entity.cpp" "src/scene/TransformSystem.cpp" "headers/scene/TransformSystem.h" "headers/scene/SceneSerializer.h" "src/scene/SceneSerializer.cpp"  "src/util/UI.cpp" "headers/util/UI.h" "headers/core/JobSystem.h" "headers/util/TsQueue.h" "headers/rendering/RenderCommon.h" "src/rendering/VkSceneRenderer.cpp" "headers/util/ByteSerializer.h" "headers/assets/AssetLoader.h" "src/assets/AssetLoader.cpp" "headers/rendering/Raytracing.h" "headers/rendering/MeshBufferManager.h" "src/rendering/MeshBufferManager.cpp" "headers/scene/TransformBufferSystem.h" "src/rendering/Raytracing.cpp" "headers/scene/RaytracingBufferSystem.h" "src/scene/RaytracingBufferSystem.cpp" "headers/resources/S_VkResource.h" "headers/resources/TLAS.h" "src/resources/TLAS.cpp" "headers/renderpasses/GBufferPass.h" "headers/scene/SceneSnapshotSystem.h" "src/scene/SceneSnapshotSystem.cpp")


target_link_options(SNAKE_VK_CORE PRIVATE -INCREMENTAL -ZI)
target_compile_options(SNAKE_VK_CORE PRIVATE -ZI)
target_link_directories(SNAKE_VK_CORE PUBLIC $ENV{VULKAN_SDK}/Lib)
target_precompile_headers(SNAKE_VK_CORE PUBLIC
headers/pch/pch.h
)

find_package(spirv_cross_cpp)

target_link_libraries(SNAKE_VK_CORE PUBLIC
${Vulkan_LIBRARIES}
${SHADERC_LIB}
GPUOpen::VulkanMemoryAllocator
spdlog::spdlog
glfw
assimp
spirv-cross-cpp
imgui
nlohmann_json::nlohmann_json
)

set(SNAKE_VK_CORE_INCLUDES 
${Vulkan_INCLUDE_DIRS}
headers
extern/spdlog/include
extern/glfw/include
extern/glm
extern/stb
extern/vma/include
extern/assimp/include
extern/entt/src/
extern/spirv-cross
extern/imgui
extern/json/include
CACHE STRING ""
)
target_include_directories(SNAKE_VK_CORE PUBLIC
${SNAKE_VK_CORE_INCLUDES}
)

set(SNAKE_VK_CORE_RESOURCE_FOLDER ${CMAKE_CURRENT_LIST_DIR}/res/ CACHE STRING "")
file(COPY ${SNAKE_VK_CORE_RESOURCE_FOLDER} DESTINATION "${CMAKE_BINARY_DIR}/Core/res")


