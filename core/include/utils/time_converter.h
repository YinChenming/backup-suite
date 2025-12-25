//
// Created by ycm on 25-9-4.
//

#ifndef TIME_CONVERTER_H
#define TIME_CONVERTER_H
#pragma once

#include <chrono>
#include <filesystem>
#include <ctime>
#include <cstdint>

#include "api.h"

namespace utils::time_converter
{

    // 将 std::filesystem::file_time_type 转换为 std::chrono::system_clock::time_point
    BACKUP_SUITE_API std::chrono::system_clock::time_point from_file_time(const std::filesystem::file_time_type& ft);

    // 将 std::chrono::system_clock::time_point 转换为 std::filesystem::file_time_type
    BACKUP_SUITE_API std::filesystem::file_time_type to_file_time(const std::chrono::system_clock::time_point& st);

} // namespace utils::time_converter

#endif // TIME_CONVERTER_H
