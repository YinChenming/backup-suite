//
// Created by ycm on 2025/9/14.
//

#ifndef BACKUPSUITE_CORE_UTILS_H
#define BACKUPSUITE_CORE_UTILS_H
#pragma once

#include <string>
#include <bitset>
#include <iostream>

#include <gtest/gtest.h>

#include "filesystem/system_device.h"

#ifdef _WIN32
#define NEWLINE (std::string("\r\n"))
#elif defined(__linux__) || defined(__APPLE__)
#define NEWLINE (std::string("\n"))
#else
#error "Unsupported platform"
static_assert(false, "Unsupported platform");
#endif

std::string get_file_type_name(FileEntityType type);

void print_meta(const FileEntityMeta& meta, std::ostream& os = std::cout);

void print_file(File& file, std::ostream& os = std::cout);

void print_folder(Folder& folder, std::ostream& os = std::cout);

bool run_script_as_admin(const std::filesystem::path& script_path);

class TestSystemDevice: public ::testing::Test
{
public:
    inline static const std::filesystem::path root = std::filesystem::temp_directory_path() / "backup_suite_tests";
    inline static const std::filesystem::path test_folder = "test_folder";
    inline static const std::filesystem::path test_write_folder = "test_write_folder";

    inline static const std::string test_file_content = "Hello, World!" + NEWLINE;
    inline static const std::string test_file_hide_content = "Hello, World!(hide)" + NEWLINE;
    inline static const std::string test_file_readonly_content = "Hello, World!(readonly)" + NEWLINE;

    inline static auto device = SystemDevice(root);
    inline static bool volatile test_symbolic_link =
#ifdef _WIN32
        false;
#elif defined(__linux__) || defined(__APPLE__)
        true;
#endif

protected:
#ifdef GTEST_NEW_SETUP_NAME
    static void SetUpTestSuite();
#else
    static void SetUpTestCase();
#endif

#ifdef GTEST_NEW_SETUP_NAME
    static void TearDownTestSuite();
#else
    static void TearDownTestCase();
#endif
};

#endif // BACKUPSUITE_CORE_UTILS_H
