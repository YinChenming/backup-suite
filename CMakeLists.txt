cmake_minimum_required(VERSION 3.20)
project(BackupSuite VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(D_CMAKE_TOOLCHAIN_FILE "C:\\Developer\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
if (EXISTS "${D_CMAKE_TOOLCHAIN_FILE}")
    include("${D_CMAKE_TOOLCHAIN_FILE}")
elseif (DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    include("${CMAKE_TOOLCHAIN_FILE}")
else ()
    message(STATUS "vcpkg toolchain not found at default; proceeding without explicit vcpkg toolchain include")
endif ()

if (MSVC)
    add_compile_options(/utf-8 /W4)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall -std=c++17 -D_POSIX_C_SOURCE=200809L)
endif ()

# 设置统一输出避免gtest找不到dll
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if (NOT EXISTS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif ()

# 覆盖Visual Studio的配置子目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

# Define per-component runtime directories under ${CMAKE_BINARY_DIR}/bin
set(BIN_ROOT "${CMAKE_BINARY_DIR}/bin")
if (NOT EXISTS "${BIN_ROOT}")
    file(MAKE_DIRECTORY "${BIN_ROOT}")
endif ()
set(BIN_CLI_DIR "${BIN_ROOT}/cli")
if (NOT EXISTS "${BIN_CLI_DIR}")
    file(MAKE_DIRECTORY "${BIN_CLI_DIR}")
endif ()
set(BIN_GUI_DIR "${BIN_ROOT}/gui")
if (NOT EXISTS "${BIN_GUI_DIR}")
    file(MAKE_DIRECTORY "${BIN_GUI_DIR}")
endif ()
set(BIN_TESTS_DIR "${BIN_ROOT}/tests")
if (NOT EXISTS "${BIN_TESTS_DIR}")
    file(MAKE_DIRECTORY "${BIN_TESTS_DIR}")
endif ()

# Ensure directories exist at configure time for IDEs that show them
file(MAKE_DIRECTORY ${BIN_ROOT})
file(MAKE_DIRECTORY ${BIN_CLI_DIR})
file(MAKE_DIRECTORY ${BIN_GUI_DIR})
file(MAKE_DIRECTORY ${BIN_TESTS_DIR})

add_subdirectory(core)
add_subdirectory(cli)
add_subdirectory(gui)

enable_testing()
add_subdirectory(tests)

# Note: To include vcpkg runtime DLLs next to the exe, configure with
# -DVCPKG_APPLOCAL_DEPS=ON so vcpkg copies dependencies into the target bin.

# CPack configuration for ZIP package
set(CPACK_GENERATOR "ZIP;NSIS")
set(CPACK_PACKAGE_NAME "BackupSuite")
set(CPACK_PACKAGE_VENDOR "BackupSuite")
if (DEFINED ENV{BACKUPSUITE_VERSION})
    # Allow overriding version via environment variable
    set(CPACK_PACKAGE_VERSION "$ENV{BACKUPSUITE_VERSION}")
else ()
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
endif ()
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-win64")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_MONOLITHIC_INSTALL OFF)

# NSIS-specific settings: offer Add to PATH and friendly naming
set(CPACK_NSIS_DISPLAY_NAME "BackupSuite CLI")
set(CPACK_NSIS_PACKAGE_NAME "BackupSuite CLI")
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_PACKAGE_EXECUTABLES "backup_suite_cli" "BackupSuite CLI")
set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CPackConfig.cmake")


# ---- Install & Packaging (CLI release ZIP) ----
include(GNUInstallDirs)

# Install CLI executable and core runtime/library into bin
install(TARGETS backup_suite_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT cli)
install(TARGETS backup_suite_core
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT core)

# Also ship any extra runtime assets already staged into bin/cli (e.g., bundled 7z)
install(DIRECTORY ${BIN_CLI_DIR}/
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT cli
        FILES_MATCHING PATTERN "*")

# Install GUI runtime and its staged assets
install(TARGETS backup_suite_gui
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT gui)
install(DIRECTORY ${BIN_GUI_DIR}/
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT gui
        FILES_MATCHING PATTERN "*")

# Include README and test_data (optional for quick verification)
install(FILES ${CMAKE_SOURCE_DIR}/README.md DESTINATION . COMPONENT common)
install(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION . COMPONENT common)

# Finalize CPack after install rules
include(CPack)

# Generate per-flavor CPack configs
configure_file(${CMAKE_SOURCE_DIR}/cmake/CPackConfigCLI.cmake.in
        ${CMAKE_BINARY_DIR}/CPackConfigCLI.cmake @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/cmake/CPackConfigGUI.cmake.in
        ${CMAKE_BINARY_DIR}/CPackConfigGUI.cmake @ONLY)

# Convenience targets to package CLI or GUI only (with core)
add_custom_target(package_cli
        COMMAND "${CMAKE_CPACK_COMMAND}" -C $<CONFIG> --config "${CMAKE_BINARY_DIR}/CPackConfigCLI.cmake"
        DEPENDS backup_suite_core backup_suite_cli)
add_custom_target(package_gui
        COMMAND "${CMAKE_CPACK_COMMAND}" -C $<CONFIG> --config "${CMAKE_BINARY_DIR}/CPackConfigGUI.cmake"
        DEPENDS backup_suite_core backup_suite_gui)
