#include "filesystem/seven_zip_device.h"
#include <utility>
#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>

SevenZipDevice::SevenZipDevice(const std::filesystem::path& path, const Mode mode, const CompressionMethod compression, const EncryptionMethod encryption, const std::vector<uint8_t>& password)
    : archive_path_(path), mode_(mode), compression_(compression), encryption_(encryption), password_(password)
{
    // 初始化后端
    backend_ = std::make_unique<P7zipBackend>();
    if (backend_)
    {
        backend_->set_compression(compression_);
        backend_->set_encryption(encryption_);
        if (!password_.empty()) backend_->set_password(password_);
        backend_->open(path, mode == Mode::ReadOnly ? ISevenZipBackend::Mode::ReadOnly : ISevenZipBackend::Mode::WriteOnly);
        is_open_ = backend_->is_open();
    }
    else
    {
        is_open_ = false;
    }
}

SevenZipDevice::~SevenZipDevice()
{
    close();
}

void SevenZipDevice::close()
{
    if (backend_) backend_->close();
    is_open_ = false;
}

void SevenZipDevice::set_password(const std::vector<uint8_t>& password)
{
    password_ = password;
    invalid_password_ = false;
    if (backend_) {
        backend_->set_password(password_);
        if (mode_ == Mode::ReadOnly) {
            backend_->close();
            backend_->open(archive_path_, ISevenZipBackend::Mode::ReadOnly);
            is_open_ = backend_->is_open();
            invalid_password_ = backend_->is_invalid_password();
        }
    }
}

void SevenZipDevice::set_compression(const CompressionMethod method)
{
    compression_ = method;
}

void SevenZipDevice::set_encryption(const EncryptionMethod method)
{
    encryption_ = method;
}

std::unique_ptr<Folder> SevenZipDevice::get_folder(const std::filesystem::path& path)
{
    if (mode_ != Mode::ReadOnly) return nullptr;
    if (!backend_) return nullptr;
    const auto entries = backend_->list_dir(path);
    if (entries.empty()) return nullptr;
    // 当前目录元数据（如缺失则以目录类型占位）
    FileEntityMeta meta{}; meta.path = path; meta.type = FileEntityType::Directory;
    std::vector<FileEntity> children;
    children.reserve(entries.size());

    // 规范化目标路径以便比较（移除尾部斜杠）
    auto normalize_path = [](const std::filesystem::path& p) {
        std::string str = p.string();
        while (!str.empty() && (str.back() == '/' || str.back() == '\\')) {
            str.pop_back();
        }
        return std::filesystem::path(str);
    };

    const auto normalized_path = normalize_path(path);

    for (const auto& e : entries)
    {
        // 规范化条目路径
        FileEntityMeta norm_meta = e.meta;
        norm_meta.path = normalize_path(e.meta.path);

        // 在 Windows 上转换路径分隔符为反斜杠
        #ifdef _WIN32
        std::string path_str = norm_meta.path.generic_string();
        std::replace(path_str.begin(), path_str.end(), '/', '\\');
        norm_meta.path = std::filesystem::path(path_str);
        #endif

        // 排除当前目录自身，避免递归时出现自包含导致无限递归
        if (norm_meta.path == normalized_path) continue;
        // 仅收集当前目录的直接子项：父路径等于当前路径
        const auto parent = normalize_path(norm_meta.path.parent_path());
        if (parent != normalized_path) continue;
        if ((norm_meta.type & FileEntityType::Directory) == FileEntityType::Directory)
            children.emplace_back(Folder{norm_meta, {}});
        else
            children.emplace_back(File{norm_meta});
    }
    // 若无直接子项但存在条目（说明 entries 仅包含深层项），仍返回空孩子的目录对象
    return std::make_unique<Folder>(meta, children);
}

std::unique_ptr<ReadableFile> SevenZipDevice::get_file(const std::filesystem::path& path)
{
    if (mode_ != Mode::ReadOnly) return nullptr;
    if (!backend_) return nullptr;
    return backend_->get_file(path);
}

std::unique_ptr<FileEntityMeta> SevenZipDevice::get_meta(const std::filesystem::path& path)
{
    if (mode_ != Mode::ReadOnly) return nullptr;
    if (!backend_) return nullptr;
    return backend_->get_meta(path);
}

bool SevenZipDevice::exists(const std::filesystem::path& path)
{
    if (mode_ != Mode::ReadOnly) return false;
    if (!backend_) return false;
    return backend_->exists(path);
}

bool SevenZipDevice::write_file(ReadableFile& file)
{
    if (mode_ != Mode::WriteOnly) return false;
    if (!backend_) return false;
    return backend_->add_file(file);
}

bool SevenZipDevice::write_file_force(ReadableFile& file)
{
    return write_file(file);
}

bool SevenZipDevice::write_folder(Folder& folder)
{
    if (mode_ != Mode::WriteOnly) return false;
    if (!backend_) return false;
    return backend_->add_folder(folder);
}
