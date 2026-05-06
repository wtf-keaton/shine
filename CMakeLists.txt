cmake_minimum_required(VERSION 3.20)

project(Shine VERSION 0.1.0 )

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "vcpkg triplet")
endif()

find_package(nlohmann_json CONFIG REQUIRED)

if (WIN32)
    find_package(unofficial-webview2 CONFIG REQUIRED)
endif()

set(SHINE_CORE_SOURCES
        core/src/app.cpp
        core/src/ipc_router.cpp
        core/src/config.cpp
)

if (WIN32)
    message(STATUS "Shine: Building for Windows")
    list(APPEND SHINE_CORE_SOURCES
            core/src/engine/win32/window.cpp
            core/src/engine/win32/webview.cpp
    )
elseif (APPLE)
    message(STATUS "Shine: Building for macOS")
    enable_language(OBJCXX)
    list(APPEND SHINE_CORE_SOURCES
            core/src/engine/macos/window.mm
            core/src/engine/macos/webview.mm
    )
elseif (UNIX AND NOT APPLE)
    message(STATUS "Shine: Building for Linux")

    list(APPEND SHINE_CORE_SOURCES
            core/src/engine/linux/window.cpp
            core/src/engine/linux/webview.cpp
    )
endif()

add_library(shine_core STATIC ${SHINE_CORE_SOURCES})

target_include_directories(shine_core
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/components/include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/core/src
)

target_link_libraries(shine_core PUBLIC nlohmann_json::nlohmann_json)

if (MSVC)
    set_target_properties(shine_core PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

if (WIN32)
    target_link_libraries(shine_core PRIVATE
            unofficial::webview2::webview2
            user32 ole32 advapi32 shlwapi
    )
elseif (APPLE)
    target_link_libraries(shine_core PRIVATE
            "-framework Cocoa"
            "-framework WebKit"
    )
elseif (UNIX AND NOT APPLE)
    # target_link_libraries(shine_core PRIVATE ${GTK3_LIBRARIES})
endif()

add_library(shine_components STATIC
        components/src/fs.cpp
        components/src/window.cpp
)

target_include_directories(shine_components PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/components/include>
)

target_link_libraries(shine_components PUBLIC shine_core)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_shine_default_build_samples ON)
else()
    set(_shine_default_build_samples OFF)
endif()

option(SHINE_ENABLE_TESTS "Build unit tests" ON)
option(SHINE_ENABLE_BENCHMARKS "Build benchmarks" ON)

if (SHINE_ENABLE_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)

    set(SHINE_UNIT_TESTS
            tests/unit/test_ipc_router.cpp
            tests/unit/test_config.cpp
    )

    add_executable(shine_unit_tests ${SHINE_UNIT_TESTS})

    target_include_directories(shine_unit_tests
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/core/include
    )

    target_link_libraries(shine_unit_tests PRIVATE
            shine_core
            GTest::gtest_main
    )

    if (MSVC)
        set_target_properties(shine_unit_tests PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(shine_unit_tests PRIVATE /W4)
    endif()

    include(GoogleTest)
    add_test(
            NAME shine_unit_tests
            COMMAND shine_unit_tests
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    set_target_properties(shine_unit_tests PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    add_custom_target(run_tests
            COMMAND shine_unit_tests
            DEPENDS shine_unit_tests
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Running Shine unit tests"
    )
endif()

if (SHINE_ENABLE_BENCHMARKS)
    if (NOT WIN32)
        message(WARNING "Shine benchmarks are currently implemented for Windows/WebView2 only")
    else()
        find_package(benchmark CONFIG REQUIRED)

        add_executable(shine_benchmarks
                benchmarks/src/memory_profiler.cpp
                benchmarks/src/benchmarks.cpp
        )

        target_include_directories(shine_benchmarks
                PRIVATE
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/include>
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
        )

        target_link_libraries(shine_benchmarks PRIVATE
                shine_core
                unofficial::webview2::webview2
                user32 ole32 advapi32 shlwapi psapi
                benchmark::benchmark
        )

        set_target_properties(shine_benchmarks PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmarks"
        )

        if (MSVC)
            set_target_properties(shine_benchmarks PROPERTIES
                    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
            )
            target_compile_options(shine_benchmarks PRIVATE /W4)
        else()
            target_compile_options(shine_benchmarks PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endif()
endif()
