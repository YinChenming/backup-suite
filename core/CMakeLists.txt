if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(VCPKG_TARGET_TRIPLET "x64-windows-dynamic" CACHE STRING "VCPKG triplet for MSVC")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "VCPKG triplet for MinGW")
endif ()

find_package(unofficial-sqlite3 REQUIRED)

set(CORE_SOURCES
        src/filesystem/system_device.cpp
        src/utils/time_converter.cpp
        src/utils/tar.cpp
        src/utils/admin_privilege.cpp
        src/filesystem/device.cpp
        src/backup/backup_controller.cpp
        src/utils/database.cpp
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

target_link_libraries(backup_suite_core PUBLIC unofficial::sqlite3::sqlite3)
