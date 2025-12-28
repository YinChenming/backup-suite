//
// Created by ycm on 2025/9/24.
//

#ifndef BACKUPSUITE_COMPRESSES_DEVICE_H
#define BACKUPSUITE_COMPRESSES_DEVICE_H

#include <memory>
#include <utility>

#include "device.h"
#include "utils/tar.h"
#include "utils/zip.h"
#include "utils/tmpfile.h"
#include "filesystem/entities.h"

template<typename T>
class BACKUP_SUITE_API IstreamReadableFile : public ReadableFile
{
    std::unique_ptr<T> is_ = nullptr;
public:
    IstreamReadableFile(const FileEntityMeta& meta, std::unique_ptr<T> is) : ReadableFile(meta), is_(std::move(is)) {}
    ~IstreamReadableFile() override = default;
    [[nodiscard]] std::unique_ptr<std::vector<std::byte>> read() override
    {
        if (!is_)
            return nullptr;
        std::vector<std::byte> buffer;
        buffer.resize(meta.size);
        is_->read(reinterpret_cast<char*>(buffer.data()), meta.size);
        const auto read_bytes = is_->gcount();
        if (is_->gcount() != static_cast<std::streamsize>(meta.size))
            return nullptr;
        return std::make_unique<std::vector<std::byte>>(std::move(buffer));
    }
    [[nodiscard]] std::unique_ptr<std::vector<std::byte>> read(size_t size) override
    {
        if (!is_ || size == 0)
            return nullptr;
        std::vector<std::byte> buffer;
        size = std::min(size, meta.size);
        buffer.resize(size);
        is_->read(reinterpret_cast<char*>(buffer.data()), size);
        if (is_->gcount() != static_cast<std::streamsize>(size))
            return nullptr;
        return std::make_unique<std::vector<std::byte>>(std::move(buffer));
    }
};

using TarIstreamReadableFile = IstreamReadableFile<tar::TarFile::TarIstream>;
using ZipIstreamReadableFile = IstreamReadableFile<zip::ZipFile::ZipIstream>;

class BACKUP_SUITE_API TarDevice : public Device
{
public:
    enum class Mode
    {
        ReadOnly,
        WriteOnly
    };

    explicit TarDevice(const std::filesystem::path& path, const Mode mode = Mode::ReadOnly)
        : mode_(mode), tar_file_(path, mode == Mode::ReadOnly ? tar::TarFile::TarMode::input : tar::TarFile::TarMode::output)
    { }

    [[nodiscard]] std::unique_ptr<Folder> get_folder(const std::filesystem::path& path) override;
    [[nodiscard]] std::unique_ptr<ReadableFile> get_file(const std::filesystem::path& path) override;
    [[nodiscard]] std::unique_ptr<FileEntityMeta> get_meta(const std::filesystem::path& path) override;
    [[nodiscard]] bool exists(const std::filesystem::path& path) override;
    [[nodiscard]] bool is_open() const { return tar_file_.is_open(); }
    bool write_file(ReadableFile& file) override;
    bool write_file_force(ReadableFile& file) override;
    bool write_folder(Folder& folder) override;
    void close()
    {
        tar_file_.close();
    }

private:
    Mode mode_;
    tar::TarFile tar_file_;

    [[nodiscard]] std::vector<std::pair<FileEntityMeta, int>> list_all_files() const;
};

class BACKUP_SUITE_API ZipDevice : public Device
{
public:
    enum class Mode
    {
        ReadOnly,
        WriteOnly
    };

    explicit ZipDevice(const std::filesystem::path& path, const Mode mode = Mode::ReadOnly,
                       const std::vector<uint8_t>& password ={})
        : mode_(mode), zip_file_(path, mode == Mode::ReadOnly ? zip::ZipFile::ZipMode::input : zip::ZipFile::ZipMode::output)
    {
        zip_file_.set_password(password);
    }

    [[nodiscard]] std::unique_ptr<Folder> get_folder(const std::filesystem::path& path) override;
    [[nodiscard]] std::unique_ptr<ReadableFile> get_file(const std::filesystem::path& path) override;
    [[nodiscard]] std::unique_ptr<FileEntityMeta> get_meta(const std::filesystem::path& path) override;
    [[nodiscard]] bool exists(const std::filesystem::path& path) override;
    [[nodiscard]] bool is_open() const { return zip_file_.is_open(); }
    bool write_file(ReadableFile& file) override;
    bool write_file(ReadableFile& file, zip::header::ZipCompressionMethod compression_method, zip::header::ZipEncryptionMethod encryption_method);
    bool write_file_force(ReadableFile& file) override;
    bool write_folder(Folder& folder) override;
    void close()
    {
        zip_file_.close();
    }
    void set_password(const std::vector<uint8_t>& password)
    {
        zip_file_.set_password(password);
    }
    void set_encryption_method(zip::header::ZipEncryptionMethod method)
    {
        encryption_method_ = method;
    }
    [[nodiscard]] bool is_invalid_password() const
    {
        return zip_file_.is_invalid_password();
    }

private:
    Mode mode_;
    zip::ZipFile zip_file_;
    zip::header::ZipCompressionMethod compression_method_ = zip::header::ZipCompressionMethod::Store;
    zip::header::ZipEncryptionMethod encryption_method_ = zip::header::ZipEncryptionMethod::Unknown;

    [[nodiscard]] std::vector<zip::ZipFile::CentralDirectoryEntry> list_all_files() const;
};

#endif // BACKUPSUITE_COMPRESSES_DEVICE_H
