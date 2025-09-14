if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(VCPKG_TARGET_TRIPLET "x64-windows-dynamic" CACHE STRING "VCPKG triplet for MSVC")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "VCPKG triplet for MinGW")
endif ()

find_package(GTest REQUIRED)

add_executable(core_tests
        src/core/test_core_device.cpp
        main.cpp
)

add_dependencies(core_tests backup_suite_core)

target_link_libraries(core_tests PRIVATE backup_suite_core
        GTest::gtest
        GTest::gtest_main
)

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
