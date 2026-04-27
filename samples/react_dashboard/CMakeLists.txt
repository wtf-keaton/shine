add_executable(react_dashboard main.cpp)

target_link_libraries(react_dashboard PRIVATE shine_core shine_components)

target_include_directories(react_dashboard PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/generated")

if (MSVC)
    target_compile_options(react_dashboard PRIVATE /utf-8)
endif()

if (MINGW)
    target_link_options(react_dashboard PRIVATE -static -static-libgcc -static-libstdc++)
endif()