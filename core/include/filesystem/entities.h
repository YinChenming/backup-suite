//
// Created by ycm on 25-9-4.
//

#ifndef FILE_ENTITY_H
#define FILE_ENTITY_H
#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>
#include <filesystem>
#include <type_traits>  // for std::underlying_type

#include "api.h"

enum class FileEntityType : std::uint8_t
{
    Unknown = 0,
    RegularFile = 1 << 0,
    Directory = 1 << 1,
    SymbolicLink = 1 << 2,
    Fifo = 1 << 3,
    Socket = 1 << 4,
    BlockDevice = 1 << 5,
    CharacterDevice = 1 << 6
};

// Enable bitwise operations for FileEntityType
constexpr FileEntityType operator|(FileEntityType a, FileEntityType b) {
    using T = std::underlying_type_t<FileEntityType>;
    return static_cast<FileEntityType>(static_cast<T>(a) | static_cast<T>(b));
}
constexpr FileEntityType operator&(FileEntityType a, FileEntityType b) {
    using T = std::underlying_type_t<FileEntityType>;
    return static_cast<FileEntityType>(static_cast<T>(a) & static_cast<T>(b));
}
constexpr FileEntityType operator^(FileEntityType a, FileEntityType b) {
    using T = std::underlying_type_t<FileEntityType>;
    return static_cast<FileEntityType>(static_cast<T>(a) ^ static_cast<T>(b));
}
constexpr FileEntityType operator~(FileEntityType a) {
    using T = std::underlying_type_t<FileEntityType>;
    return static_cast<FileEntityType>(~static_cast<T>(a));
}

struct FileEntityMeta
{
    std::filesystem::path path;
    FileEntityType type = FileEntityType::Unknown;
    size_t size = 0;

    std::chrono::system_clock::time_point creation_time;
    std::chrono::system_clock::time_point modification_time;
    std::chrono::system_clock::time_point access_time;

    // POSIX 特有元数据
    uint32_t posix_mode = 0;
    uint32_t uid = 0;
    uint32_t gid = 0;

    std::string user_name, group_name;

    // Windows 特有元数据
    uint32_t windows_attributes = 0;

    // 特殊文件元数据
    std::filesystem::path symbolic_link_target;
    uint32_t device_major = 0;
    uint32_t device_minor = 0;
};

BACKUP_SUITE_API void update_file_entity_meta(FileEntityMeta& meta);

class BACKUP_SUITE_API FileEntity
{
protected:
    FileEntityMeta meta = {};
public:
    virtual ~FileEntity() = default;
    FileEntity() = default;
    explicit FileEntity(FileEntityMeta metaData): meta(std::move(metaData)) {}
    FileEntityMeta& get_meta() { return meta; }
};

class BACKUP_SUITE_API Folder: public FileEntity
{
protected:
    std::vector<FileEntity> children = {};
public:
    ~Folder() override = default;
    Folder(const FileEntityMeta& metaData, const std::vector<FileEntity>& childrenData)
        : FileEntity(metaData), children(childrenData) {}
    std::vector<FileEntity>& get_children() { return children; }
};

class BACKUP_SUITE_API File: public FileEntity
{
public:
    ~File() override = default;
    File() = default;
    explicit File(const FileEntityMeta& metaData): FileEntity(metaData) {}
};

class BACKUP_SUITE_API ReadableFile : public File
{
public:
    ~ReadableFile() override = default;
    ReadableFile() = default;
    explicit ReadableFile(File &file): File(file.get_meta()) {}
    explicit ReadableFile(const FileEntityMeta& metaData): File(metaData) {}
    [[nodiscard]] virtual std::unique_ptr<std::vector<std::byte>> read() = 0;
    [[nodiscard]] virtual std::unique_ptr<std::vector<std::byte>> read(size_t size) = 0;
    virtual void close() {};
};

class BACKUP_SUITE_API EmptyReadableFile : public ReadableFile
{
public:
    ~EmptyReadableFile() override = default;
    EmptyReadableFile() = default;
    explicit EmptyReadableFile(File &file): ReadableFile(file) {}
    explicit EmptyReadableFile(const FileEntityMeta& meta) : ReadableFile(meta) {}
    [[nodiscard]] std::unique_ptr<std::vector<std::byte>> read() override { return nullptr; }
    [[nodiscard]] std::unique_ptr<std::vector<std::byte>> read(size_t) override { return nullptr; }
};

#endif //FILE_ENTITY_H