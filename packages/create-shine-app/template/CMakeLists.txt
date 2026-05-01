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
if (WIN32)
    set_property(TARGET ${TARGET_NAME} PROPERTY WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options({{PROJECT_NAME}} PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
    endif()
endif ()

shine_embed_config(demo-shine-app "${CMAKE_CURRENT_SOURCE_DIR}/shine.conf.json")

shine_attach_frontend({{PROJECT_NAME}})