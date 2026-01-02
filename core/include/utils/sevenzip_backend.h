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
    class MemoryReadableFile : public ReadableFile {
        std::shared_ptr<std::vector<std::byte>> data_{};
        size_t cursor_ = 0;
    public:
        MemoryReadableFile(const FileEntityMeta& meta, std::shared_ptr<std::vector<std::byte>> data)
            : ReadableFile(meta), data_(std::move(data)) {}
        [[nodiscard]] std::unique_ptr<std::vector<std::byte>> read() override {
            if (!data_ || cursor_ >= data_->size()) return nullptr;
            auto out = std::make_unique<std::vector<std::byte>>(data_->begin() + static_cast<long long>(cursor_), data_->end());
            cursor_ = data_->size();
            return out;
        }
        [[nodiscard]] std::unique_ptr<std::vector<std::byte>> read(const size_t size) override {
            if (!data_ || cursor_ >= data_->size() || size == 0) return nullptr;
            const auto remain = data_->size() - cursor_;
            const auto n = (std::min)(remain, size);
            auto out = std::make_unique<std::vector<std::byte>>(data_->begin() + static_cast<long long>(cursor_), data_->begin() + static_cast<long long>(cursor_ + n));
            cursor_ += n;
            return out;
        }
        void close() override { cursor_ = data_ ? data_->size() : 0; }
    };

    std::filesystem::path archive_{};
    Mode mode_ = Mode::ReadOnly;
    bool open_ = false;
    bool invalid_password_ = false;
    std::vector<uint8_t> password_{};
    sevenzip::CompressionMethod compression_ = sevenzip::CompressionMethod::LZMA2;
    sevenzip::EncryptionMethod encryption_ = sevenzip::EncryptionMethod::None;
    // libarchive-based read-only index
    bool disk_index_built_ = false;
    std::unordered_map<std::filesystem::path, FileEntityMeta> disk_meta_{};
    // staging area for write mode to produce a real .7z via external 7z
    std::filesystem::path staging_dir_{};

#ifdef _WIN32
    static bool run_7z_command(const std::vector<std::wstring>& args);
#else
    bool run_7z_command(const std::vector<std::string>& args);
#endif
    [[nodiscard]] static std::string build_7z_command_line(const std::vector<std::string>& args);
    bool get_file_impl_extract(const std::filesystem::path& normalized_query,
                               const std::shared_ptr<std::vector<std::byte>>& out_data);
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

    // Helper utilities to resolve and run the external 7z CLI
    static bool ensure_sevenz_cli();
    static std::filesystem::path sevenz_cli();
};

#endif // BACKUPSUITE_SEVENZIP_BACKEND_H
