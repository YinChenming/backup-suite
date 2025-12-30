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

add_executable(core_tests
        main.cpp
        ${CORE_TEST_SOURCE}
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
gtest_discover_tests(core_tests)

# 定义可执行文件
# Placeholder app: off by default and excluded from ALL_BUILD
option(BUILD_PLACEHOLDER_MYPROJECT "Build placeholder MyProject target" OFF)
if (BUILD_PLACEHOLDER_MYPROJECT)
    add_executable(MyProject EXCLUDE_FROM_ALL main.cpp)
    target_link_libraries(MyProject PRIVATE GTest::gtest GTest::gtest_main)
    target_include_directories(MyProject PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif ()

if(WIN32 AND BUILD_PLACEHOLDER_MYPROJECT AND TARGET MyProject)
    if(MSVC)
        # MSVC 方案：直接通过连接器参数设置
        set_target_properties(MyProject PROPERTIES
                LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
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
        target_sources(MyProject PRIVATE ${RC_FILE})
    endif()
endif()