if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(VCPKG_TARGET_TRIPLET "x64-windows-dynamic" CACHE STRING "VCPKG triplet for MSVC")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "VCPKG triplet for MinGW")
endif ()

find_package(GTest CONFIG REQUIRED)
find_package(LibArchive REQUIRED)

set(CORE_TEST_SOURCE
        src/core/test_core_device.cpp
        src/core/core_utils.cpp
        src/core/test_core_backup_suite.cpp
        src/core/test_core_compress.cpp
        src/core/test_core_compress_device.cpp
        src/core/test_core_encrypt.cpp
        src/core/test_core_seven_zip_all.cpp
)

set(ENABLE_7Z_TESTS ${BACKUPSUITE_ENABLE_7Z_TESTS})
find_program(SEVENZ_CLI NAMES 7z 7za
        HINTS ${CMAKE_SOURCE_DIR}/third_party/7-Zip ${CMAKE_SOURCE_DIR}/third_party/7-Zip/bin ${CMAKE_SOURCE_DIR}/third_party/7zip ${CMAKE_SOURCE_DIR}/third_party/7zip/bin)
if (ENABLE_7Z_TESTS AND NOT SEVENZ_CLI)
    message(STATUS "7z/7za not found; disabling seven_zip tests")
    set(ENABLE_7Z_TESTS OFF)
endif ()

add_executable(core_tests
        main.cpp
        ${CORE_TEST_SOURCE}
)

# Put test executable in BIN_ROOT so it can load shared DLLs located there during discovery
set_target_properties(core_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BIN_ROOT}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_ROOT}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_ROOT}
)

target_include_directories(core_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_dependencies(core_tests backup_suite_core)

target_link_libraries(core_tests PRIVATE backup_suite_core
        GTest::gtest
        GTest::gtest_main
)

target_link_libraries(core_tests PRIVATE LibArchive::LibArchive)

# 将项目根路径传递到测试，便于在测试中定位 test_data
target_compile_definitions(core_tests PRIVATE PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")

if (WIN32)
    target_link_libraries(core_tests PRIVATE Shell32.lib)
endif ()

if (GTest_VERSION VERSION_GREATER_EQUAL "1.8.1")
    # 如果 GTest 版本大于等于 1.8.1，就定义GTEST_NEW_SETUP_NAME宏，使用新的SetUpTestSuite和TearDownTestSuite名称
    message(STATUS "GTest version ${GTest_VERSION} found. Using modern SetUpTestSuite names.")
    target_compile_definitions(core_tests PRIVATE GTEST_NEW_SETUP_NAME=1)
else ()
    message(STATUS "GTest version ${GTest_VERSION} found. Using legacy SetUpTestCase names.")
endif ()

include(GoogleTest)
# Run test discovery with working directory where shared DLLs are present so discovery executable can load dependencies
gtest_discover_tests(core_tests WORKING_DIRECTORY ${BIN_ROOT})

# 定义可执行文件
# Placeholder app: off by default and excluded from ALL_BUILD
option(BUILD_PLACEHOLDER_MYPROJECT "Build placeholder core_tests target" OFF)
if (BUILD_PLACEHOLDER_MYPROJECT)
    add_executable(core_tests EXCLUDE_FROM_ALL main.cpp)
    target_link_libraries(core_tests PRIVATE GTest::gtest GTest::gtest_main)
    target_include_directories(core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif ()

if (WIN32 AND BUILD_PLACEHOLDER_MYPROJECT AND TARGET core_tests)
    if (MSVC)
        # MSVC 方案：直接通过连接器参数设置
        set_target_properties(core_tests PROPERTIES
                LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
        )
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # MinGW/GCC 方案：需要创建一个 .rc 资源文件并编译进 exe

        # 1. 创建 manifest 内容
        set(MANIFEST_FILE "${CMAKE_CURRENT_BINARY_DIR}/admin.manifest")
        file(WRITE ${MANIFEST_FILE}
                "<?xml version='1.0' encoding='UTF-8' standalone='yes'?>\n"
                "<assembly xmlns='urn:schemas-microsoft-com:asm.v1' manifestVersion='1.0'>\n"
                "  <trustInfo xmlns='urn:schemas-microsoft-com:asm.v3'>\n"
                "    <security>\n"
                "      <requestedPrivileges>\n"
                "        <requestedExecutionLevel level='requireAdministrator' uiAccess='false' />\n"
                "      </requestedPrivileges>\n"
                "    </security>\n"
                "  </trustInfo>\n"
                "</assembly>")

        # 2. 创建 .rc 资源文件内容
        set(RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/admin.rc")
        file(WRITE ${RC_FILE} "1 24 \"${MANIFEST_FILE}\"")

        # 3. 将资源文件加入编译列表
        target_sources(core_tests PRIVATE ${RC_FILE})
    endif ()
endif ()

# Ensure core DLL and a copy of the test exe are available in tests runtime dir
if (TARGET backup_suite_core)
    add_custom_command(TARGET core_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Copying core_tests.exe and backup_suite_core DLL to ${BIN_TESTS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:core_tests> ${BIN_TESTS_DIR}
    )
endif ()