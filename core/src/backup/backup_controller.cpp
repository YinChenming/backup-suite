//
// Created by ycm on 2025/9/14.
//
#include "backup/backup_controller.h"
#include <iostream>
#include <filesystem>

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
        // 获取源设备的根目录
        auto root_folder = from.get_folder("");
        if (!root_folder) {
            return false;
        }

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

        // 遍历文件夹中的所有子项
        for (auto& child : folder->get_children()) {
            const auto& child_meta = child.get_meta();

            // 根据配置过滤文件类型
            if (!(static_cast<unsigned int>(child_meta.type) & static_cast<unsigned int>(config.backup_file_types))) {
                continue;
            }

            if (child_meta.type == FileEntityType::Directory) {
                // 递归处理子目录
                if (!copy_folder_recursive(from, to, child_meta.path)) {
                    return false;
                }
            } else if (child_meta.type == FileEntityType::RegularFile) {
                // 复制文件
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
        }

        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}
