//
// Created by GitHub Copilot on 2025/12/29.
//

#ifndef BACKUPSUITE_SEVEN_ZIP_DEVICE_H
#define BACKUPSUITE_SEVEN_ZIP_DEVICE_H
#pragma once

#include <vector>
#include <cstdint>
#include <filesystem>

#include "api.h"
#include "filesystem/device.h"
#include "filesystem/entities.h"
#include "utils/sevenzip_backend.h"

using sevenzip::CompressionMethod;
using sevenzip::EncryptionMethod;

class BACKUP_SUITE_API SevenZipDevice : public Device
{
public:
    enum class Mode
    {
        ReadOnly,
        WriteOnly
    };

    explicit SevenZipDevice(const std::filesystem::path& path,
                            Mode mode = Mode::ReadOnly,
                            CompressionMethod compression = CompressionMethod::LZMA2,
                            EncryptionMethod encryption = EncryptionMethod::None,
                            const std::vector<uint8_t>& password = {});

    ~SevenZipDevice() override;

    // Device overrides
    [[nodiscard]] std::unique_ptr<Folder> get_folder(const std::filesystem::path& path) override;
    [[nodiscard]] std::unique_ptr<ReadableFile> get_file(const std::filesystem::path& path) override;
    [[nodiscard]] std::unique_ptr<FileEntityMeta> get_meta(const std::filesystem::path& path) override;
    [[nodiscard]] bool exists(const std::filesystem::path& path) override;
    bool write_file(ReadableFile& file) override;
    bool write_file_force(ReadableFile& file) override;
    bool write_folder(Folder& folder) override;

    // Lifecycle
    void close();
    [[nodiscard]] bool is_open() const { return is_open_; }

    // Configuration
    void set_password(const std::vector<uint8_t>& password);
    void set_compression(CompressionMethod method);
    void set_encryption(EncryptionMethod method);
    [[nodiscard]] bool is_invalid_password() const { return invalid_password_; }

private:
    std::filesystem::path archive_path_{};
    Mode mode_ = Mode::ReadOnly;
    CompressionMethod compression_ = CompressionMethod::LZMA;
    EncryptionMethod encryption_ = EncryptionMethod::None;
    std::vector<uint8_t> password_{};
    bool is_open_ = false;
    bool invalid_password_ = false;
    std::unique_ptr<ISevenZipBackend> backend_{};
};

#endif // BACKUPSUITE_SEVEN_ZIP_DEVICE_H
