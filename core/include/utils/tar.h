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

namespace tar
{
    class BACKUP_SUITE_API TarInitializationStrategy final : public db::DatabaseInitializationStrategy
    {
    public: using SQLEntity = std::tuple<
            std::string,    // path
            int,            // type
            int,            // size
            int,            // offset
            long long,      // creation_time
            long long,      // modification_time
            long long,      // access_time
            int,            // posix_mode
            int,            // uid
            int,            // gid
            std::string,    // user_name
            std::string,    // group_name
            int,            // windows_attributes
            std::string,    // symbolic_link_target
            int,            // device_major
            int             // device_minor
        >;
        inline const static std::string SQLEntityColumns = (" path, type, size, offset, creation_time, modification_time, "
                                                        "access_time, posix_mode, uid, gid, user_name, group_name, "
                                                        "windows_attributes, symbolic_link_target, device_major, "
                                                        "device_minor ");
    protected:
        [[nodiscard]] std::string get_initialization_sql() const override
        {
            return (
                "CREATE TABLE IF NOT EXISTS entity_type("
                "id INTEGER PRIMARY KEY UNIQUE,"
                "name TEXT NOT NULL UNIQUE"
                ");"

                "INSERT INTO entity_type (id, name) VALUES "
                "(0, 'unknown'),"
                "(1, 'regular_file'),"
                "(2, 'directory'),"
                "(4, 'symbolic_link'),"
                "(8, 'fifo'),"
                "(16, 'socket'),"
                "(32, 'block_device'),"
                "(64, 'character_device')"
                ";"

                "CREATE TABLE IF NOT EXISTS entity("
                "id INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE,"
                "path TEXT NOT NULL,"
                "type INTEGER NOT NULL DEFAULT 0,"
                "size INTEGER NOT NULL,"
                "offset INTEGER NOT NULL DEFAULT 0,"
                "creation_time DATETIME NOT NULL,"
                "modification_time DATETIME NOT NULL,"
                "access_time DATETIME NOT NULL,"
                "posix_mode INTEGER NOT NULL DEFAULT 0,"
                "uid INTEGER NOT NULL DEFAULT 0,"
                "gid INTEGER NOT NULL DEFAULT 0,"
                "user_name TEXT NOT NULL DEFAULT '',"
                "group_name TEXT NOT NULL DEFAULT '',"
                "windows_attributes INTEGER NOT NULL DEFAULT 0,"
                "symbolic_link_target TEXT,"
                "device_major INTEGER DEFAULT 0,"
                "device_minor INTEGER DEFAULT 0,"
                "FOREIGN KEY(type) REFERENCES entity_type(id) ON DELETE CASCADE"
                ");"
            );
        }
    };

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

    template <typename T>
    struct BACKUP_SUITE_API FStreamDeleter
    {
        virtual ~FStreamDeleter() = default;
        using pointer = T*;

        [[nodiscard]] virtual bool need_delete() const
        {
            return true;
        }

        void operator()(T* stream) const
        {
            if (stream && stream->is_open())
            {
                stream->close();
            }
            if (need_delete())
                delete stream;
        }
    };

    class BACKUP_SUITE_API TarFile
    {
        class TarIstreamBuf: public std::streambuf
        {
            std::ifstream &file_;
            int offset_;
            size_t size_;
            static constexpr size_t buffer_size_ = 8192;
            char buffer_[buffer_size_] = {};
        public: explicit TarIstreamBuf(std::ifstream &ifs, int offset, size_t size):
            file_(ifs), offset_(offset), size_(size)
        {
            setg(nullptr, nullptr, nullptr);
            if (offset < 0 || size == 0 || !ifs.is_open() || ifs.eof())
            {
                offset_ = size_ = 0;
                return;
            }
        }
            ~TarIstreamBuf() override = default;
            int_type underflow() override
            {
                if (gptr() < egptr())
                {
                    return traits_type::to_int_type(*gptr());
                }
                file_.seekg(offset_, std::ios::beg);
                size_t to_read = std::min(buffer_size_, size_);
                file_.read(buffer_, to_read);
                offset_ += to_read;
                size_ -= to_read;
                if (to_read == 0 || file_.eof() || file_.gcount() != static_cast<std::streamsize>(to_read))
                {
                    return traits_type::eof();
                }
                setg(buffer_, buffer_, buffer_ + to_read);
                return traits_type::to_int_type(*gptr());
            }
        };

        class TarIstream: public std::istream
        {
            FileEntityMeta meta_;
            std::ifstream &file_;
            std::unique_ptr<TarIstreamBuf> buffer_;
        public:
            TarIstream(std::ifstream &ifs, const int offset, const FileEntityMeta &meta):
                std::istream(nullptr), meta_(meta), file_(ifs), buffer_(std::make_unique<TarIstreamBuf>(ifs, offset, meta.size))
            {
                rdbuf(buffer_.get());
                if (!meta_.size || !ifs.is_open() || ifs.eof())
                {
                    setstate(std::ios::eofbit);
                }
            }
            ~TarIstream() override = default;
            FileEntityMeta get_meta() { return meta_; }
        };
        using IFStreamPointer = std::unique_ptr<std::ifstream, FStreamDeleter<std::ifstream>>;
        using OFStreamPointer = std::unique_ptr<std::ofstream, FStreamDeleter<std::ofstream>>;

        static constexpr int TarBlockSize = sizeof(TarBlock);   // 512 bytes

        db::Database db_;
        std::filesystem::path path_;
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
        TarFile(std::filesystem::path path, const TarMode mode, const FStreamDeleter<std::ifstream>& ifsDeleter = FStreamDeleter<std::ifstream>(),
                       const FStreamDeleter<std::ofstream>& ofsDeleter = FStreamDeleter<std::ofstream>()) :
          db_(std::move(std::make_unique<TarInitializationStrategy>()).get()), path_(std::move(path)), ifs_(nullptr, ifsDeleter), ofs_(nullptr, ofsDeleter)
        {
            if (mode == TarMode::input)
            {
                ifs_ = IFStreamPointer(new std::ifstream(path_, std::ios::binary), FStreamDeleter<std::ifstream>());
                init_db_from_tar();
            } else
            {
                ofs_ = OFStreamPointer(new std::ofstream(path_, std::ios::binary | std::ios::trunc), FStreamDeleter<std::ofstream>());
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
        void close() const;
    };
}
#endif // TAR_H
