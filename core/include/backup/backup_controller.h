//
// Created by ycm on 2025/9/8.
//

#ifndef BACKUPSUITE_BACKUP_CONTROLLER_H
#define BACKUPSUITE_BACKUP_CONTROLLER_H
#pragma once

#include <queue>
#include <regex>
#include <chrono>

#include "api.h"
#include "filesystem/device.h"

struct BackupConfig
{
    bool backup_symbolic_links = false;
    FileEntityType backup_file_types = FileEntityType::RegularFile | FileEntityType::Directory | FileEntityType::SymbolicLink;

    // 路径过滤（支持通配符或正则表达式）
    std::vector<std::string> include_patterns;  // 包含的路径模式
    std::vector<std::string> exclude_patterns;  // 排除的路径模式
    bool use_regex = false;  // 使用正则表达式而不是通配符

    // 文件扩展名过滤
    std::vector<std::string> include_extensions;  // 包含的扩展名（如 ".txt", ".cpp"）
    std::vector<std::string> exclude_extensions;  // 排除的扩展名

    // 时间过滤
    bool filter_by_time = false;
    std::chrono::system_clock::time_point time_after;   // 修改时间晚于此时间
    std::chrono::system_clock::time_point time_before;  // 修改时间早于此时间

    // 文件大小过滤（字节）
    bool filter_by_size = false;
    uint64_t min_size = 0;      // 最小文件大小
    uint64_t max_size = UINT64_MAX;  // 最大文件大小

    // POSIX 权限过滤
    bool filter_by_permissions = false;
    uint32_t required_permissions = 0;  // 需要的权限位
    uint32_t excluded_permissions = 0;  // 排除的权限位

    // 用户/组过滤
    std::vector<std::string> include_users;   // 包含的用户名
    std::vector<std::string> exclude_users;   // 排除的用户名
    std::vector<std::string> include_groups;  // 包含的组名
    std::vector<std::string> exclude_groups;  // 排除的组名
};

class BACKUP_SUITE_API BackupController
{
    BackupConfig config = {};
public:
    BackupController() = default;
    // ReSharper disable once CppNonExplicitConvertingConstructor
    BackupController(const BackupConfig& cfg): config(cfg) {} // NOLINT(*-explicit-constructor)
    void run_backup(Device& from, Device& to) const;
    bool run_restore(Device& from, Device& to) const;
private:
    bool copy_folder_recursive(Device& from, Device& to, const std::filesystem::path& path) const;
    bool should_backup_file(const FileEntityMeta& meta) const;  // 检查文件是否应该备份
    bool match_pattern(const std::string& path, const std::string& pattern) const;  // 路径模式匹配
};

#endif // BACKUPSUITE_BACKUP_CONTROLLER_H
