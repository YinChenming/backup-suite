//
// SevenZip backend abstraction for 7z read/write
//

#ifndef BACKUPSUITE_SEVENZIP_BACKEND_H
#define BACKUPSUITE_SEVENZIP_BACKEND_H
#pragma once

#include <filesystem>
#include <memory>
#include <vector>
#include <unordered_map>

#include "api.h"
#include "filesystem/entities.h"

namespace sevenzip
{
    enum class CompressionMethod
    {
        LZMA,
        LZMA2,
        Deflate
    };

    enum class EncryptionMethod
    {
        None,
        AES128,
        AES192,
        AES256
    };

    struct DirEntry
    {
        FileEntityMeta meta;
        // Used algorithms discovered on read (for write, reflects configured choices)
        CompressionMethod used_compression = CompressionMethod::LZMA2;
        EncryptionMethod used_encryption = EncryptionMethod::None;
        // additional flags can be added here if needed
    };
}

class BACKUP_SUITE_API ISevenZipBackend
{
public:
    enum class Mode { ReadOnly, WriteOnly };
    virtual ~ISevenZipBackend() = default;
    virtual bool open(const std::filesystem::path& archive, Mode mode) = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool is_open() const = 0;
    [[nodiscard]] virtual bool is_invalid_password() const = 0;

    virtual void set_password(const std::vector<uint8_t>& password) = 0;
    virtual void set_compression(sevenzip::CompressionMethod) = 0;
    virtual void set_encryption(sevenzip::EncryptionMethod) = 0;

    virtual std::vector<sevenzip::DirEntry> list_dir(const std::filesystem::path& path) = 0;
    virtual std::unique_ptr<ReadableFile> get_file(const std::filesystem::path& path) = 0;
    virtual std::unique_ptr<FileEntityMeta> get_meta(const std::filesystem::path& path) = 0;
    virtual bool exists(const std::filesystem::path& path) = 0;
    virtual bool add_file(ReadableFile& file) = 0;
    virtual bool add_folder(Folder& folder) = 0;
};

// Placeholder p7zip-backed implementation; real integration to be filled next.
class BACKUP_SUITE_API P7zipBackend : public ISevenZipBackend
{
    std::filesystem::path archive_{};
    Mode mode_ = Mode::ReadOnly;
    bool open_ = false;
    bool invalid_password_ = false;
    std::vector<uint8_t> password_{};
    sevenzip::CompressionMethod compression_ = sevenzip::CompressionMethod::LZMA2;
    sevenzip::EncryptionMethod encryption_ = sevenzip::EncryptionMethod::None;
    std::string sevenz_cli_; // resolved path to 7z/7za for write mode
    // libarchive-based read-only index
    bool disk_index_built_ = false;
    std::unordered_map<std::filesystem::path, FileEntityMeta> disk_meta_{};
    // staging area for write mode to produce a real .7z via external 7z
    std::filesystem::path staging_dir_{};
public:
    bool open(const std::filesystem::path& archive, Mode mode) override;
    void close() override;
    [[nodiscard]] bool is_open() const override;
    [[nodiscard]] bool is_invalid_password() const override;
    void set_password(const std::vector<uint8_t>& password) override;
    void set_compression(sevenzip::CompressionMethod) override;
    void set_encryption(sevenzip::EncryptionMethod) override;
    std::vector<sevenzip::DirEntry> list_dir(const std::filesystem::path& path) override;
    std::unique_ptr<ReadableFile> get_file(const std::filesystem::path& path) override;
    std::unique_ptr<FileEntityMeta> get_meta(const std::filesystem::path& path) override;
    bool exists(const std::filesystem::path& path) override;
    bool add_file(ReadableFile& file) override;
    bool add_folder(Folder& folder) override;
};

#endif // BACKUPSUITE_SEVENZIP_BACKEND_H
