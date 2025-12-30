//
// Created by ycm on 2025/9/14.
//
#include "backup/backup_controller.h"
#include <filesystem>
#include <regex>
#include <algorithm>

void BackupController::run_backup(Device& from, Device& to) const
{
    std::queue<std::unique_ptr<Folder>> queue;
    queue.push(from.get_folder(""));

    while (!queue.empty())
    {
        auto folder = std::move(queue.front());
        queue.pop();
        if (!folder)
            continue;
        to.write_folder(*folder);
        for (auto &child: folder->get_children())
        {
            const auto meta = child.get_meta();
            if (!(static_cast<unsigned int>(meta.type) & static_cast<unsigned int>(config.backup_file_types)))
                continue;
            // 应用过滤条件
            if (!should_backup_file(meta))
                continue;
            if (meta.type == FileEntityType::Directory)
            {
                queue.push(from.get_folder(meta.path));
                continue;
            }
            std::unique_ptr<ReadableFile> tmp_file = from.get_file(meta.path);
            if (!tmp_file)
                continue;
            if (tmp_file->get_meta().type != FileEntityType::Directory)
            {
                to.write_file(*tmp_file);
            }
            tmp_file->close();
        }
    }
}

bool BackupController::run_restore(Device& from, Device& to) const
{
    try {
        // 确保目标目录为空或存在
        auto target_meta = to.get_meta("");
        if (target_meta && target_meta->type != FileEntityType::Unknown) {
            // 目标不为空，可以选择清空或跳过
        }

        return copy_folder_recursive(from, to, "");
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool BackupController::copy_folder_recursive(Device& from, Device& to, const std::filesystem::path& path) const
{
    try {
        // 获取当前路径的文件夹
        auto folder = from.get_folder(path);
        if (!folder) {
            return false;
        }

        // 确保目标文件夹存在
        if (path != "") {
            to.write_folder(*folder);
        }

        // 为了避免在未创建父目录前写入文件，优先处理目录，其次处理文件
        std::vector<FileEntity> dirs;
        std::vector<FileEntity> files;
        dirs.reserve(folder->get_children().size());
        files.reserve(folder->get_children().size());
        for (auto& child : folder->get_children()) {
            const auto& child_meta = child.get_meta();
            if (!(static_cast<unsigned int>(child_meta.type) & static_cast<unsigned int>(config.backup_file_types))) {
                continue;
            }
            // 应用过滤条件
            if (!should_backup_file(child_meta)) {
                continue;
            }
            if (child_meta.type == FileEntityType::Directory) {
                dirs.emplace_back(child);
            }
            else if (child_meta.type == FileEntityType::RegularFile) {
                files.emplace_back(child);
            }
        }

        // 先递归处理目录
        for (auto& child : dirs) {
            const auto& child_meta = child.get_meta();
            if (!copy_folder_recursive(from, to, child_meta.path)) {
                return false;
            }
        }
        // 再处理文件
        for (auto& child : files) {
            const auto& child_meta = child.get_meta();
            auto source_file = from.get_file(child_meta.path);
            if (!source_file) {
                continue;
            }
            if (!to.write_file_force(*source_file)) {
                source_file->close();
                return false;
            }
            source_file->close();
        }

        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool BackupController::should_backup_file(const FileEntityMeta& meta) const
{
    // 使用正斜杠进行路径匹配（跨平台）
    std::string path_str = meta.path.string();
    // 将反斜杠转换为正斜杠，便于统一的路径匹配
    std::replace(path_str.begin(), path_str.end(), '\\', '/');

    // 1. 检查路径包含模式
    if (!config.include_patterns.empty()) {
        bool matched = false;
        for (const auto& pattern : config.include_patterns) {
            if (match_pattern(path_str, pattern)) {
                matched = true;
                break;
            }
        }
        if (!matched) return false;
    }

    // 2. 检查路径排除模式
    for (const auto& pattern : config.exclude_patterns) {
        if (match_pattern(path_str, pattern)) {
            return false;
        }
    }

    // 3. 检查文件扩展名包含
    if (!config.include_extensions.empty() && meta.type == FileEntityType::RegularFile) {
        const std::string ext = meta.path.extension().string();
        bool matched = false;
        for (const auto& inc_ext : config.include_extensions) {
            if (ext == inc_ext || (inc_ext[0] != '.' && ext == "." + inc_ext)) {
                matched = true;
                break;
            }
        }
        if (!matched) return false;
    }

    // 4. 检查文件扩展名排除
    if (!config.exclude_extensions.empty() && meta.type == FileEntityType::RegularFile) {
        const std::string ext = meta.path.extension().string();
        for (const auto& exc_ext : config.exclude_extensions) {
            if (ext == exc_ext || (exc_ext[0] != '.' && ext == "." + exc_ext)) {
                return false;
            }
        }
    }

    // 5. 检查修改时间
    if (config.filter_by_time) {
        if (meta.modification_time < config.time_after ||
            meta.modification_time > config.time_before) {
            return false;
        }
    }

    // 6. 检查文件大小
    if (config.filter_by_size && meta.type == FileEntityType::RegularFile) {
        if (meta.size < config.min_size || meta.size > config.max_size) {
            return false;
        }
    }

    // 7. 检查权限
    if (config.filter_by_permissions) {
        if ((meta.posix_mode & config.required_permissions) != config.required_permissions) {
            return false;
        }
        if ((meta.posix_mode & config.excluded_permissions) != 0) {
            return false;
        }
    }

    // 8. 检查用户名包含
    if (!config.include_users.empty()) {
        bool matched = false;
        for (const auto& user : config.include_users) {
            if (meta.user_name == user) {
                matched = true;
                break;
            }
        }
        if (!matched) return false;
    }

    // 9. 检查用户名排除
    for (const auto& user : config.exclude_users) {
        if (meta.user_name == user) {
            return false;
        }
    }

    // 10. 检查组名包含
    if (!config.include_groups.empty()) {
        bool matched = false;
        for (const auto& group : config.include_groups) {
            if (meta.group_name == group) {
                matched = true;
                break;
            }
        }
        if (!matched) return false;
    }

    // 11. 检查组名排除
    for (const auto& group : config.exclude_groups) {
        if (meta.group_name == group) {
            return false;
        }
    }

    return true;
}

bool BackupController::match_pattern(const std::string& path, const std::string& pattern) const
{
    if (config.use_regex) {
        try {
            std::regex re(pattern, std::regex::icase);
            return std::regex_search(path, re);
        } catch (const std::regex_error&) {
            return false;
        }
    } else {
        // 简单的通配符匹配: * 匹配任意字符序列, ? 匹配单个字符
        std::string regex_pattern;

        // 检查是否包含通配符
        bool has_wildcards = pattern.find('*') != std::string::npos ||
                            pattern.find('?') != std::string::npos;

        // 构建正则表达式：转义特殊字符，但保留通配符
        const std::string special_chars = ".^$+()[]{}|\\";
        for (char c : pattern) {
            if (c == '*') {
                regex_pattern += ".*";
            } else if (c == '?') {
                regex_pattern += ".";
            } else if (special_chars.find(c) != std::string::npos) {
                regex_pattern += '\\';
                regex_pattern += c;
            } else {
                regex_pattern += c;
            }
        }

        // 如果没有通配符，将模式作为子字符串搜索（两端添加 .*）
        // 这样 "subdir" 会匹配 "subdir" 或 "subdir/file.txt"
        if (!has_wildcards) {
            regex_pattern = ".*" + regex_pattern + ".*";
        }

        try {
            std::regex re(regex_pattern, std::regex::icase);
            return std::regex_match(path, re);
        } catch (const std::regex_error&) {
            return false;
        }
    }
}
