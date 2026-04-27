cmake_minimum_required(VERSION 3.20)

project({{PROJECT_NAME}} VERSION 0.1.0 )

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(shine)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable({{PROJECT_NAME}} src/main.cpp)

target_link_libraries({{PROJECT_NAME}} PRIVATE shine_core shine_components)

target_include_directories({{PROJECT_NAME}} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/generated")
