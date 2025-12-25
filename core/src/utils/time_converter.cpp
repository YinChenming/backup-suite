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
BACKUP_SUITE_API time_t dos_to_unix_time(const uint16_t dos_date, const uint16_t dos_time) {
    struct tm t;

    // 清零以防受到本地时区/夏令时干扰
    t.tm_isdst = -1;

    // 解析时间
    t.tm_sec  = (dos_time & 0x1F) * 2;      // Bit 0-4
    t.tm_min  = (dos_time >> 5) & 0x3F;     // Bit 5-10
    t.tm_hour = (dos_time >> 11) & 0x1F;    // Bit 11-15

    // 解析日期
    t.tm_mday = (dos_date & 0x1F);          // Bit 0-4
    t.tm_mon  = ((dos_date >> 5) & 0x0F) - 1; // Bit 5-8 (tm_mon 是 0-11)
    t.tm_year = ((dos_date >> 9) & 0x7F) + 80; // Bit 9-15 (1980偏移, tm_year 是从 1900 开始)

    return mktime(&t); // 返回 Unix Timestamp
}