function(shine_attach_frontend TARGET_NAME)
    set(FRONTEND_DIR "${CMAKE_CURRENT_SOURCE_DIR}/frontend")
    set(SCRIPTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/scripts")
    set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
    set(OUTPUT_HEADER "${GENERATED_DIR}/embedded_assets.hpp")

    if(WIN32)
        find_program(NPM_EXECUTABLE NAMES npm.cmd npm REQUIRED)
    else()
        find_program(NPM_EXECUTABLE NAMES npm REQUIRED)
    endif()

    message(STATUS "[Shine] Checking frontend dependencies for target: ${TARGET_NAME}")
    message(STATUS "[Shine] Npm Executable: ${NPM_EXECUTABLE}")

    execute_process(
            COMMAND ${NPM_EXECUTABLE} install
            WORKING_DIRECTORY ${FRONTEND_DIR}
            RESULT_VARIABLE NPM_INSTALL_RESULT
            OUTPUT_QUIET
    )

    if(NOT NPM_INSTALL_RESULT EQUAL 0)
        message(FATAL_ERROR "[Shine] Failed to install npm dependencies!")
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        message(STATUS "[Shine] Release mode: Frontend will be built and embedded.")

        add_custom_command(
                OUTPUT "${OUTPUT_HEADER}"
                COMMAND "${NPM_EXECUTABLE}" --prefix "${FRONTEND_DIR}" run build
                COMMAND node "${SCRIPTS_DIR}/embed-assets.mjs"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                COMMENT "[Shine] Compiling React Frontend and packing assets..."
        )

        set(ASSETS_TARGET "${TARGET_NAME}_Assets")
        add_custom_target(${ASSETS_TARGET} DEPENDS "${OUTPUT_HEADER}")
        add_dependencies(${TARGET_NAME} ${ASSETS_TARGET})

    else()
        message(STATUS "[Shine] Debug mode: Verifying Vite dev server status...")

        set(ASSETS_TARGET "${TARGET_NAME}_DevServer")

        add_custom_target(${ASSETS_TARGET}
                COMMAND node "${SCRIPTS_DIR}/dev-server.mjs" "${NPM_EXECUTABLE}"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "[Shine] Checking and managing Vite dev server..."
        )

        add_dependencies(${TARGET_NAME} ${ASSETS_TARGET})

    endif()

    target_include_directories(${TARGET_NAME} PRIVATE "${GENERATED_DIR}")
endfunction()

function(shine_embed_config TARGET_NAME CONFIG_FILE)
    set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(OUTPUT_HEADER "${GENERATED_DIR}/shine_config.hpp")

    file(MAKE_DIRECTORY "${GENERATED_DIR}")

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CONFIG_FILE}")

    file(READ "${CONFIG_FILE}" JSON_CONTENT)

    set(CPP_CODE
            "#pragma once
namespace shine::embedded {
    constexpr const char* kConfig = R\"SHINE_JSON(
${JSON_CONTENT}
)SHINE_JSON\";
}
")

    file(WRITE "${OUTPUT_HEADER}" "${CPP_CODE}")

    target_include_directories(${TARGET_NAME} PRIVATE "${GENERATED_DIR}")
endfunction()