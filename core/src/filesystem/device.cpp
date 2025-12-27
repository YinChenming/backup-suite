//
// Created by ycm on 2025/9/14.
//
#include "filesystem/device.h"

std::unique_ptr<ReadableFile> PhysicalDevice::get_file(const std::filesystem::path& path)
{
    const auto meta = get_meta(path);
    if (!meta || meta->type != FileEntityType::RegularFile)
        return nullptr;
    std::unique_ptr<std::ifstream> stream = get_file_stream(path);
    if (!stream)
        return nullptr;
    return std::make_unique<PhysicalDeviceReadableFile>(*meta, std::move(stream));
}


std::unique_ptr<std::vector<std::byte>> PhysicalDeviceReadableFile::read()
{
    if (!stream || !stream->good())
        return nullptr;
    auto buffer = std::make_unique<std::vector<std::byte>>(meta.size);
    stream->read(reinterpret_cast<char*>(buffer->data()), meta.size);
    if (stream->gcount() != static_cast<std::streamsize>(meta.size))
        return nullptr;
    return buffer;
}

std::unique_ptr<std::vector<std::byte>> PhysicalDeviceReadableFile::read(size_t size)
{
    if (!stream || !stream->good() || size == 0)
        return nullptr;
    size = std::min(size, meta.size);
    auto buffer = std::make_unique<std::vector<std::byte>>(size);
    stream->read(reinterpret_cast<char*>(buffer->data()), size);
    if (stream->gcount() != static_cast<std::streamsize>(size))
        return nullptr;
    return buffer;
}
