cmake_minimum_required(VERSION 3.8)

project(SNAKE_VK_EDITOR)

add_executable(SNAKE_VK_EDITOR
"src/main.cpp"
 "src/EditorLayer.cpp" "headers/EntityEditor.h" "src/EntityEditor.cpp" "headers/AssetEditor.h" "src/AssetEditor.cpp" )

target_link_libraries(SNAKE_VK_EDITOR PUBLIC SNAKE_VK_CORE)

target_include_directories(SNAKE_VK_EDITOR PUBLIC 
${SNAKE_VK_CORE_INCLUDES}
)

file(COPY ${SNAKE_VK_CORE_RESOURCE_FOLDER} DESTINATION "${CMAKE_BINARY_DIR}/Editor/res")
file(COPY ${SNAKE_VK_CORE_REQUIRED_BINARIES} DESTINATION "${CMAKE_BINARY_DIR}/Editor")
file(COPY res/ DESTINATION "${CMAKE_BINARY_DIR}/Editor/res")
