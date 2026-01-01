//
// Created by ycm on 2025/12/27.
//

#ifndef BACKUPSUITE_TMP_FILE_H
#define BACKUPSUITE_TMP_FILE_H

#include <filesystem>
#include <memory>
#include <utility>

#include "api.h"

namespace tmpfile_utils
{
    /*
     * WARNING! Please DO NOT try to read the file path directly,
    */
    class BACKUP_SUITE_API TmpFile
    {
    public:
        explicit TmpFile(std::filesystem::path  path) : path_(std::move(path)) {}

        // RAII：析构时自动删除文件
        ~TmpFile() {
            if (!path_.empty() && std::filesystem::exists(path_)) {
                try
                {
                    std::filesystem::remove(path_);
                } catch (...) {}
            }
        }
        // 禁止拷贝，只允许移动（所有权唯一）
        TmpFile(const TmpFile&) = delete;
        TmpFile& operator=(const TmpFile&) = delete;
        TmpFile(TmpFile&& other) noexcept : path_(std::move(other.path_)) { other.path_.clear(); }

        [[nodiscard]] const auto& path() const { return path_; }
        static std::unique_ptr<TmpFile> create();
    private:
        std::filesystem::path path_;
    };
}

#endif // BACKUPSUITE_TMP_FILE_H
