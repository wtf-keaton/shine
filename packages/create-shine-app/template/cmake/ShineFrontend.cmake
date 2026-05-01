function(shine_attach_frontend TARGET_NAME)
    set(FRONTEND_DIR "${CMAKE_CURRENT_SOURCE_DIR}/frontend")
    set(SCRIPTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/scripts")
    set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
    set(OUTPUT_HEADER "${GENERATED_DIR}/embedded_assets.hpp")

    find_program(NPM_EXECUTABLE NAMES npm npm.cmd REQUIRED)

    message(STATUS "[Shine] Checking frontend dependencies for target: ${TARGET_NAME}")

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
                COMMAND ${NPM_EXECUTABLE} run build
                WORKING_DIRECTORY ${FRONTEND_DIR}
                COMMAND node "${SCRIPTS_DIR}/embed-assets.mjs"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "[Shine] Compiling React Frontend and packing assets..."
        )

        set(ASSETS_TARGET "${TARGET_NAME}_Assets")
        add_custom_target(${ASSETS_TARGET} DEPENDS "${OUTPUT_HEADER}")

        add_dependencies(${TARGET_NAME} ${ASSETS_TARGET})

    else()
        message(STATUS "[Shine] Debug mode: Starting Vite dev server in background...")

        if(WIN32)
            execute_process(
                    COMMAND cmd /c start /b ${NPM_EXECUTABLE} run dev
                    WORKING_DIRECTORY ${FRONTEND_DIR}
            )
        else()
            execute_process(
                    COMMAND ${NPM_EXECUTABLE} run dev &
                    WORKING_DIRECTORY ${FRONTEND_DIR}
            )
        endif()
    endif()

    target_include_directories(${TARGET_NAME} PRIVATE "${GENERATED_DIR}")
endfunction()