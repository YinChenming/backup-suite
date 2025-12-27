//
// Created by ycm on 25-9-5.
//

#ifndef TAR_H
#define TAR_H
#pragma once

#include <filesystem>
#include <fstream>
#include <utility>
#include <map>
#include <algorithm>
// ReSharper disable once CppUnusedIncludeDirective
#include <cstring>

#include "api.h"
#include "filesystem/entities.h"
#include "database.h"
#include "utils/fs_deleter.h"
#include "utils/streams.h"
#include "utils/database_strategies.h"

using fs_deleter::FStreamDeleter;
using fs_deleter::IFStreamPointer;
using fs_deleter::OFStreamPointer;

namespace tar
{
    using TarInitializationStrategy = db::TarInitializationStrategy;

    enum class TarStandard
    {
        UNKNOWN,
        POSIX_1988_USTAR,
        POSIX_2001_PAX,
        GNU,
    };
#ifdef _MSC_VER
// warning C4201: 使用了非标准扩展: 无名称的结构/联合
#pragma warning(disable: 4201)
#endif
    // POSIX.1-1988 (ustar) tar file header
    // POSIX IEEE P1003.1
    struct BACKUP_SUITE_API TarFileHeader
    {
        char name[100];
        char mode[8];
        char uid[8];
        char gid[8];
        char size[12];
        char mtime[12];
        char checksum[8];
        char type_flag;
        char link_name[100];
        union
        {
            struct
            {
                char magic[6];  // "ustar"
                char version[2]; // "00"
            };
            char reserved[8];
        } ustar;
        char uname[32];
        char gname[32];
        char device_major[8];
        char device_minor[8];
        char prefix[155];
        char padding[12];
    };

    union TarBlock
    {
        TarFileHeader header;
        char block[512];
    };

    class BACKUP_SUITE_API TarFile
    {
    public:
        using TarIstreamBuf = IstreamBuf;
        using TarIstream = FileEntityIstream;

        static constexpr int TarBlockSize = sizeof(TarBlock);   // 512 bytes
    private:
        db::Database db_;
        IFStreamPointer ifs_;
        OFStreamPointer ofs_;
        bool is_valid_ = true;
        TarStandard standard_ = TarStandard::UNKNOWN;
    protected:
        static FileEntityMeta tar_header2file_meta(const TarFileHeader &header, TarStandard standard = TarStandard::GNU);
        static TarFileHeader file_meta2tar_header(const FileEntityMeta &meta, TarStandard standard = TarStandard::GNU);
        static std::pair<FileEntityMeta, int> sql_entity2file_meta(const TarInitializationStrategy::SQLEntity& entity);
        [[nodiscard]] bool insert_entity(const FileEntityMeta& meta, int offset) const;
        void init_db_from_tar();
    public:
        enum TarMode
        {
            input,
            output
        };
        explicit TarFile(const std::filesystem::path& path, const FStreamDeleter<std::ifstream>& ifsDeleter = FStreamDeleter<std::ifstream>(),
                       const FStreamDeleter<std::ofstream>& ofsDeleter = FStreamDeleter<std::ofstream>())
        {
            if (std::filesystem::exists(path))
            {
                TarFile(path, TarMode::input, ifsDeleter, ofsDeleter);
            } else
            {
                TarFile(path, TarMode::output, ifsDeleter, ofsDeleter);
            }
        }
        TarFile(const std::filesystem::path& path, const TarMode mode, const FStreamDeleter<std::ifstream>& ifsDeleter = FStreamDeleter<std::ifstream>(),
                       const FStreamDeleter<std::ofstream>& ofsDeleter = FStreamDeleter<std::ofstream>()) :
          db_(std::move(std::make_unique<TarInitializationStrategy>()).get()), ifs_(nullptr, ifsDeleter), ofs_(nullptr, ofsDeleter)
        {
            if (mode == TarMode::input)
            {
                ifs_ = IFStreamPointer(new std::ifstream(path, std::ios::binary), FStreamDeleter<std::ifstream>());
                if (!ifs_ || !ifs_->is_open())
                {
                    is_valid_ = false;
                    return;
                }
                init_db_from_tar();
            } else
            {
                ofs_ = OFStreamPointer(new std::ofstream(path, std::ios::binary | std::ios::trunc), FStreamDeleter<std::ofstream>());
                // Check if the file was successfully opened
                if (!ofs_ || !ofs_->is_open())
                    is_valid_ = false;
            }
        }
        [[nodiscard]] std::unique_ptr<TarIstream> get_file_stream(const std::filesystem::path& path) const;
        [[nodiscard]] std::vector<std::pair<FileEntityMeta, int>> list_dir(const std::filesystem::path& path) const;
        void set_standard(const TarStandard standard){standard_ = standard;}
        [[nodiscard]] TarStandard get_standard() const { return standard_; }

        bool add_entity(ReadableFile& file);
        // 关闭文件流,在output模式中表示将数据写入文件中,在input模式中表示单纯的关闭文件流,应当在~TarFile()中自动调用
        void close();
        [[nodiscard]] bool is_open() const { return is_valid_; }
        [[nodiscard]] bool readable() const { return is_valid_ && ifs_ && ifs_->is_open(); }
        [[nodiscard]] bool writable() const { return is_valid_ && ofs_ && ofs_->is_open(); }
    };
}
#endif // TAR_H
