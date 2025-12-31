set(CLI_SOURCES
        src/main.cpp
)

add_executable(backup_suite_cli
        ${CLI_SOURCES}
)

# Place runtime output into the shared per-component bin
set_target_properties(backup_suite_cli PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BIN_CLI_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_CLI_DIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_CLI_DIR}
)

target_include_directories(backup_suite_cli
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
)

target_link_libraries(backup_suite_cli PRIVATE backup_suite_core)

# Ensure core DLL is available next to CLI executable after build
if (TARGET backup_suite_core)
    add_custom_command(TARGET backup_suite_cli POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Copying backup_suite_core DLL to ${BIN_CLI_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:backup_suite_core> ${BIN_CLI_DIR}
    )
endif ()
