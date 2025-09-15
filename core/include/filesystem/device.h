//
// Created by ycm on 25-9-4.
//

#ifndef DEVICE_H
#define DEVICE_H
#pragma once

#include <filesystem>
#include <fstream>

#include "api.h"
#include "entities.h"

// 取消 MSVC 中定义的 min 和 max 宏
#undef min
#undef max
#include <algorithm>

class PhysicalDeviceReadableFile: public ReadableFile
{
    std::unique_ptr<std::ifstream> stream = nullptr;
public:
    PhysicalDeviceReadableFile(File& file, std::unique_ptr<std::ifstream> fs): ReadableFile(file)
    {
        stream = std::move(fs);
    }
    PhysicalDeviceReadableFile(const FileEntityMeta &metaData, std::unique_ptr<std::ifstream> fs): ReadableFile(metaData)
    {
        stream = std::move(fs);
    }
    ~PhysicalDeviceReadableFile() override{
        this->PhysicalDeviceReadableFile::close();
    };
    std::ifstream &get_stream() { return *stream; }
    [[nodiscard]] std::unique_ptr<std::vector<std::byte>> read() override;
    [[nodiscard]] std::unique_ptr<std::vector<std::byte>> read(size_t size) override;
    void close() override
    {
        if (stream && stream->is_open())
            stream->close();
    }
};

class BACKUP_SUITE_API Device
{
public:
    static constexpr size_t CACHE_SIZE = 1024 * 1024;
    virtual ~Device() = default;
    [[nodiscard]] virtual std::unique_ptr<Folder> get_folder(const std::filesystem::path &path) const = 0;
    [[nodiscard]] virtual std::unique_ptr<ReadableFile> get_file(const std::filesystem::path &path) const = 0;
    [[nodiscard]] virtual std::unique_ptr<FileEntityMeta> get_meta(const std::filesystem::path &path) const = 0;
    [[nodiscard]] virtual bool exists(const std::filesystem::path& path) const = 0;
    virtual bool write_file(ReadableFile &file) = 0;
    virtual bool write_file_force(ReadableFile &file) = 0;
    virtual bool write_folder(Folder &folder) = 0;
};

class BACKUP_SUITE_API PhysicalDevice: public Device
{
public:
    static constexpr size_t CACHE_SIZE = 1024 * 1024;
    ~PhysicalDevice() override = default;
    [[nodiscard]] std::unique_ptr<ReadableFile> get_file(const std::filesystem::path& path) const override;
    [[nodiscard]] virtual std::unique_ptr<std::ifstream> get_file_stream(const std::filesystem::path& path) const = 0;
};

class BACKUP_SUITE_API DeviceDecorator: public Device
{
protected:
    std::shared_ptr<Device> device = nullptr;
public:
    DeviceDecorator() = default;
    explicit DeviceDecorator(const std::shared_ptr<Device> &device) : device(device) {}
    ~DeviceDecorator() override = default;
    [[nodiscard]] std::unique_ptr<Folder> get_folder(const std::filesystem::path& path) const override
    {
        return device->get_folder(path);
    }
    [[nodiscard]] std::unique_ptr<ReadableFile> get_file(const std::filesystem::path& path) const override
    {
        return device->get_file(path);
    }
    [[nodiscard]] std::unique_ptr<FileEntityMeta> get_meta(const std::filesystem::path& path) const override
    {
        return device->get_meta(path);
    }
    [[nodiscard]] bool exists(const std::filesystem::path& path) const override
    {
        if (!device->exists(path))
            return false;
        return is_valid(*device->get_meta(path));
    }
    bool write_file(ReadableFile &file) override
    {
        return device->write_file(file);
    }
    bool write_file_force(ReadableFile &file) override
    {
        return device->write_file_force(file);
    }
    bool write_folder(Folder &folder) override
    {
        return device->write_folder(folder);
    }
    void set_device(const std::shared_ptr<Device>& new_device)
    {
        device = new_device;
    }
    [[nodiscard]] std::shared_ptr<Device> get_device() const
    {
        return device;
    }
    [[nodiscard]] virtual bool is_valid(const FileEntityMeta& path) const = 0;
};

#endif // DEVICE_H
