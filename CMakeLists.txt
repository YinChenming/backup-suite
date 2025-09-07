cmake_minimum_required(VERSION 3.20)
project(BackupSuite)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/utf-8 /W4)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall -std=c++17 -D_POSIX_C_SOURCE=200809L)
endif ()

# 设置统一输出避免gtest找不到dll
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_subdirectory(core)
add_subdirectory(cli)
add_subdirectory(gui)

enable_testing()
add_subdirectory(tests)
