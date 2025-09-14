//
// Created by ycm on 2025/9/14.
//
#include "backup/backup_controller.h"

void BackupSuite::run_backup(const Device &from, Device &to) const
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
            to.write_file(*tmp_file);
        }
    }
}
