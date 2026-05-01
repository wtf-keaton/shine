cmake_minimum_required(VERSION 3.20)

project({{PROJECT_NAME}} VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(cmake/ShineFrontend.cmake)

add_subdirectory(shine)

add_executable({{PROJECT_NAME}} src/main.cpp)

target_link_libraries({{PROJECT_NAME}} PRIVATE shine_core shine_components)

shine_attach_frontend({{PROJECT_NAME}})