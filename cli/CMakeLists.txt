include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)


set(CLI_SOURCES
        # cpp files
        src/main.cpp
)
add_executable(backup_suite_cli
        ${CLI_SOURCES}
)

target_link_libraries(backup_suite_cli PRIVATE backup_suite_core)
