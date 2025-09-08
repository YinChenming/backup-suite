//
// Created by ycm on 25-9-4.
//

#ifndef FILE_ENTITY_H
#define FILE_ENTITY_H

#include <iostream>
#include <utility>
#include <vector>
#include <filesystem>

#include "../api.h"

enum class FileEntityType
{
    RegularFile,
    Directory,
    SymbolicLink,
    Fifo,
    Socket,
    BlockDevice,
    CharacterDevice,
    Unknown
};

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

class BACKUP_SUITE_API FileEntity
{
protected:
    FileEntityMeta meta = {};
public:
    virtual ~FileEntity() = default;
    FileEntity() = default;
    explicit FileEntity(FileEntityMeta  metaData): meta(std::move(metaData)) {}
    FileEntityMeta& get_meta() { return meta; }
};

class BACKUP_SUITE_API Folder: public FileEntity
{
protected:
    std::vector<FileEntity> children;
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
    [[nodiscard]] virtual std::vector<std::byte> read() = 0;
    [[nodiscard]] virtual std::vector<std::byte> read(size_t size) = 0;
    virtual void close() {};
};

#endif //FILE_ENTITY_H
