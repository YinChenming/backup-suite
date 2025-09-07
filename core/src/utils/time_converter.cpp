//
// Created by ycm on 25-9-4.
//
#include "utils/time_converter.h"

using namespace utils::time_converter;
// 将 std::filesystem::file_time_type 转换为 std::chrono::system_clock::time_point
BACKUP_SUITE_API std::chrono::system_clock::time_point
from_file_time(const std::filesystem::file_time_type& ft)
{
    const auto duration = ft.time_since_epoch();
    return std::chrono::system_clock::time_point(duration);
}

// 将 std::chrono::system_clock::time_point 转换为 std::filesystem::file_time_type
BACKUP_SUITE_API std::filesystem::file_time_type to_file_time(const std::chrono::system_clock::time_point& st)
{
    const auto duration = st.time_since_epoch();
    return std::filesystem::file_time_type(duration);
}
