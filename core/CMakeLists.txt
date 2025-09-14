set(CORE_SOURCES
        src/filesystem/system_device.cpp
        src/utils/time_converter.cpp
        src/utils/tar.cpp
        src/utils/admin_privilege.cpp
        src/filesystem/device.cpp
        src/backup/backup_controller.cpp
)

# 将include目录添加到项目中
add_library(backup_suite_core SHARED
        ${CORE_SOURCES}
)

target_compile_definitions(backup_suite_core PRIVATE BACKUP_SUITE_CORE_EXPORTS)

target_include_directories(backup_suite_core
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
