set(CLI_SOURCES
        src/main.cpp
)

add_executable(backup_suite_cli
        ${CLI_SOURCES}
)

target_include_directories(backup_suite_cli
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
)

target_link_libraries(backup_suite_cli PRIVATE backup_suite_core)
