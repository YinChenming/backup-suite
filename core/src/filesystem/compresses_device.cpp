//
// Created by ycm on 2025/12/27.
//

#include "filesystem/compresses_device.h"
#include <algorithm>
#include <chrono>

std::vector<std::pair<FileEntityMeta, int>> TarDevice::list_all_files() const
{
    std::vector<std::pair<FileEntityMeta, int>> result;
    if (mode_ != Mode::ReadOnly) {
        return result;
    }

    // 列出根目录下的所有文件
    const auto files = tar_file_.list_dir({});
    for (const auto& file_pair : files) {
        result.push_back(file_pair);
    }

    return result;
}
std::unique_ptr<Folder> TarDevice::get_folder(const std::filesystem::path& path) const
{
    if (!tar_file_.is_open() || mode_ != Mode::ReadOnly) {
        return nullptr;
    }

    // 我们需要对根目录做特殊处理,因为tar中不存在一个根目录,我们需要伪造一个根目录
    if (path.empty() || path == "." || path == "./")
    {
        const auto files = tar_file_.list_dir({});
        std::vector<FileEntity> children;
        for (auto &[entity, offset] : files) {
            const std::string entity_path = entity.path.generic_u8string();
            const auto l_pos = entity_path.find('/'),
            r_pos = entity_path.rfind('/');
            if (l_pos == std::string::npos)
            {
                // 根目录下的目录
                children.emplace_back(entity);
                continue;
            }
            if (l_pos == r_pos && l_pos == entity_path.size() - 1)
            {
                // 直接子节点
                children.emplace_back(entity);
                continue;
            }
        }
        FileEntityMeta root_meta;
        root_meta.path = ".";
        root_meta.type = FileEntityType::Directory;
        return std::make_unique<Folder>(root_meta, children);
    }
    const auto fstream = tar_file_.get_file_stream(path);
    if (fstream == nullptr) return nullptr;
    FileEntityMeta meta = fstream->get_meta();
    auto files = tar_file_.list_dir(path);
    std::vector<FileEntity> children;
    for (auto &[entity, offset] : files) {
        if (path == entity.path)
        {
            continue;
        }
        children.emplace_back(entity);
    }
    return std::make_unique<Folder>(meta, children);
}
std::unique_ptr<ReadableFile> TarDevice::get_file(const std::filesystem::path& path) const
{
    if (!is_open() || mode_ != Mode::ReadOnly) {
        return nullptr;
    }

    auto file_stream = tar_file_.get_file_stream(path);
    if (file_stream == nullptr) return nullptr;
    if (const std::string path_str = file_stream->get_meta().path.generic_u8string(); path_str[path_str.size()-1] == '/')
    {
        // 目录文件
        return nullptr;
    }
    return std::make_unique<TarIstreamReadableFile>(file_stream->get_meta(), std::move(file_stream));
}
std::unique_ptr<FileEntityMeta> TarDevice::get_meta(const std::filesystem::path& path) const
{
    if (!is_open() || mode_ != Mode::ReadOnly) {
        return nullptr;
    }

    if (const auto stream = tar_file_.get_file_stream(path))
    {
        return std::make_unique<FileEntityMeta>(stream->get_meta());
    } else
    {
        return nullptr;
    }
}
bool TarDevice::exists(const std::filesystem::path& path) const
{
    auto meta = get_meta(path);
    return meta != nullptr;
}
bool TarDevice::write_file(ReadableFile& file)
{
    if (!is_open() || mode_ != Mode::WriteOnly) {
        return false;
    }
    return tar_file_.add_entity(file);
}
bool TarDevice::write_file_force(ReadableFile& file)
{
    return write_file(file); // Tar格式不支持强制覆盖，使用相同的实现
}
bool TarDevice::write_folder(Folder& folder)
{
    if (!is_open() || mode_ != Mode::WriteOnly) {
        return false;
    }

    // 我们需要对根目录做特殊处理,因为tar中不存在一个根目录,我们拦截根目录的写入请求
    // 如果是根目录,直接返回**成功**,因为tar本身就是根目录
    if (const auto path = folder.get_meta().path; path.empty() || path == "." || path == "./") return true;
    EmptyReadableFile folder_file{folder.get_meta()};
    return tar_file_.add_entity(folder_file);
}

std::vector<zip::ZipFile::CentralDirectoryEntry> ZipDevice::list_all_files() const
{
    if (mode_ != Mode::ReadOnly)
    {
        return {};
    }
    return zip_file_.list_dir({});
}
std::unique_ptr<Folder> ZipDevice::get_folder(const std::filesystem::path& path) const
{
    if (!is_open() || mode_ != Mode::ReadOnly) {
        return nullptr;
    }

    // 我们需要对根目录做特殊处理,因为zip中不存在一个根目录,我们需要伪造一个根目录
    if (path.empty() || path == "." || path == "./")
    {
        const auto files = zip_file_.list_dir({});
        std::vector<FileEntity> children;
        for (const auto& entry : files)
        {
            const std::string entry_path = entry.file_name;
            const auto l_pos = entry_path.find('/'),
            r_pos = entry_path.rfind('/');
            if (l_pos == std::string::npos)
            {
                // 根目录下的目录
                children.emplace_back(zip::ZipFile::cdfh_to_file_meta(entry));
                continue;
            }
            if (l_pos == r_pos && l_pos == entry_path.size() - 1)
            {
                // 直接子节点
                children.emplace_back(zip::ZipFile::cdfh_to_file_meta(entry));
            }
        }
        FileEntityMeta root_meta;
        root_meta.path = ".";
        root_meta.type = FileEntityType::Directory;
        return std::make_unique<Folder>(root_meta, children);
    }
    const auto fstream = zip_file_.get_file_stream(path);
    if (fstream == nullptr) return nullptr;
    FileEntityMeta meta = fstream->get_meta();
    const auto files = zip_file_.list_dir(path);
    std::vector<FileEntity> children;
    for (auto &entry : files)
    {
        if (entry.file_name == path){
            continue;
        }
        children.emplace_back(zip::ZipFile::cdfh_to_file_meta(entry));
    }
    return std::make_unique<Folder>(meta, children);
}
std::unique_ptr<ReadableFile> ZipDevice::get_file(const std::filesystem::path& path) const
{
    if (!is_open() || mode_ != Mode::ReadOnly) {
        return nullptr;
    }

    auto file_stream = zip_file_.get_file_stream(path);
    if (file_stream == nullptr) return nullptr;
    if (const std::string path_str = file_stream->get_meta().path.generic_u8string(); path_str[path_str.size()-1] == '/')
    {
        // 目录文件
        return nullptr;
    }
    return std::make_unique<ZipIstreamReadableFile>(file_stream->get_meta(), std::move(file_stream));
}
std::unique_ptr<FileEntityMeta> ZipDevice::get_meta(const std::filesystem::path& path) const
{
    if (!is_open() || mode_ != Mode::ReadOnly) {
        return nullptr;
    }

    auto stream = zip_file_.get_file_stream(path);
    if (stream == nullptr) return nullptr;
    else return std::make_unique<FileEntityMeta>(stream->get_meta());
}
bool ZipDevice::exists(const std::filesystem::path& path) const
{
    const auto meta = get_meta(path);
    return meta != nullptr;
}
bool ZipDevice::write_file(ReadableFile& file, zip::header::ZipCompressionMethod compression_method)
{
    if (!is_open() || mode_ != Mode::WriteOnly) {
        return false;
    }
    return zip_file_.add_entity(file, compression_method);
}
bool ZipDevice::write_file(ReadableFile& file)
{
    return write_file(file, compression_method_);
}
bool ZipDevice::write_file_force(ReadableFile& file)
{
    return write_file(file); // Zip格式不支持强制覆盖，使用相同的实现
}
bool ZipDevice::write_folder(Folder& folder)
{
    if (!is_open() || mode_ != Mode::WriteOnly) {
        return false;
    }

    // 我们需要对根目录做特殊处理,因为zip中不存在一个根目录,我们拦截根目录的写入请求
    // 如果是根目录,直接返回**成功**,因为zip本身就是根目录
    if (const auto path = folder.get_meta().path; path.empty() || path == "." || path == "./") return true;
    EmptyReadableFile folder_file{folder.get_meta()};
    return zip_file_.add_entity(folder_file, zip::header::ZipCompressionMethod::Store);
}