//
// Created by ycm on 25-9-4.
//

#ifndef DEVICE_H
#define DEVICE_H

#include <iostream>
#include <filesystem>
#include <fstream>

#include "../api.h"
#include "entities.h"

class PhysicalDeviceReadableFile: public ReadableFile
{
    std::unique_ptr<std::ifstream> stream;
public:
    PhysicalDeviceReadableFile(const FileEntityMeta &metaData, std::unique_ptr<std::ifstream> fs): stream(std::move(fs))
    {
        meta = metaData;
    }
    ~PhysicalDeviceReadableFile() override{
        this->PhysicalDeviceReadableFile::close();
    };
    std::ifstream &get_stream() { return *stream; }
    [[nodiscard]] std::vector<std::byte> read() override
    {
        if (!stream || !stream->good())
            return {};
        const auto buffer = std::make_unique<std::vector<std::byte>>(meta.size);
        stream->read(reinterpret_cast<char*>(buffer->data()), meta.size);
        if (stream->gcount() != static_cast<std::streamsize>(meta.size))
            return {};
        return *buffer;
    }
    [[nodiscard]] std::vector<std::byte> read(size_t size) override
    {
        if (!stream || !stream->good() || size == 0)
            return {};
        const auto buffer = std::make_unique<std::vector<std::byte>>(size);
        stream->read(reinterpret_cast<char*>(buffer->data()), size);
        if (stream->gcount() != static_cast<std::streamsize>(size))
            return {};
        return *buffer;
    }
    void close() override
    {
        if (stream && stream->is_open())
        {
            stream->close();
        }
    }
};

class BACKUP_SUITE_API Device
{
public:
    static constexpr size_t CACHE_SIZE = 1024 * 1024;
    virtual ~Device() = default;
    [[nodiscard]] virtual std::unique_ptr<Folder> get_folder(const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual std::unique_ptr<ReadableFile> get_file(const std::filesystem::path& path) const =0;
    [[nodiscard]] virtual std::unique_ptr<FileEntityMeta> get_meta(const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual bool exists(const std::filesystem::path& path) const = 0;
    virtual bool write_file(const ReadableFile &) = 0;
    virtual bool write_folder(const Folder &folder) = 0;
};

class BACKUP_SUITE_API PhysicalDevice: Device
{
public:
    static constexpr size_t CACHE_SIZE = 1024 * 1024;
    ~PhysicalDevice() override = default;
    [[nodiscard]] std::unique_ptr<ReadableFile> get_file(const std::filesystem::path& path) const override{
        const auto meta = get_meta(path);
        if (!meta || meta->type != FileEntityType::RegularFile)
            return nullptr;
        std::unique_ptr<std::ifstream> stream = get_file_stream(path);
        if (!stream)
            return nullptr;
        return std::make_unique<PhysicalDeviceReadableFile>(*meta, std::move(stream));
    }
    [[nodiscard]] virtual std::unique_ptr<std::ifstream> get_file_stream(const std::filesystem::path& path) const = 0;
};

#endif // DEVICE_H
