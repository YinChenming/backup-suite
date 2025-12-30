cmake_minimum_required(VERSION 3.20)
project(BackupSuite VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(D_CMAKE_TOOLCHAIN_FILE "C:\\Developer\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
if (EXISTS "${D_CMAKE_TOOLCHAIN_FILE}")
    include("${D_CMAKE_TOOLCHAIN_FILE}")
elseif (DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    include("${CMAKE_TOOLCHAIN_FILE}")
else()
    message(STATUS "vcpkg toolchain not found at default; proceeding without explicit vcpkg toolchain include")
endif()

if (MSVC)
    add_compile_options(/utf-8 /W4)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall -std=c++17 -D_POSIX_C_SOURCE=200809L)
endif ()

# 设置统一输出避免gtest找不到dll
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# 覆盖Visual Studio的配置子目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

add_subdirectory(core)
add_subdirectory(cli)
add_subdirectory(gui)

enable_testing()
add_subdirectory(tests)

# ---- Install & Packaging (CLI release ZIP) ----
include(GNUInstallDirs)

# Install CLI executable and core runtime/library into bin
install(TARGETS backup_suite_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS backup_suite_core
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Include README and test_data (optional for quick verification)
install(FILES ${CMAKE_SOURCE_DIR}/README.md DESTINATION .)
if (EXISTS ${CMAKE_SOURCE_DIR}/test_data)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/test_data DESTINATION .)
endif()

# Note: To include vcpkg runtime DLLs next to the exe, configure with
# -DVCPKG_APPLOCAL_DEPS=ON so vcpkg copies dependencies into the target bin.

# CPack configuration for ZIP package
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "BackupSuite")
set(CPACK_PACKAGE_VENDOR "BackupSuite")
if (DEFINED ENV{BACKUPSUITE_VERSION})
    # Allow overriding version via environment variable
    set(CPACK_PACKAGE_VERSION "$ENV{BACKUPSUITE_VERSION}")
else()
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
endif()
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-win64")
include(CPack)
