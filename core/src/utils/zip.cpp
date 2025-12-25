//
// Created by ycm on 2025/10/1.
//

#include "utils/zip.h"
#include "utils/crc.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <utility>

using namespace zip;
using namespace zip::header;



// 构造函数
ZipFile::ZipFile(std::string  zip_path, const bool overwrite)
    : m_zip_path(std::move(zip_path)), m_closed(false)
{
    std::ios_base::openmode mode = std::ios::out | std::ios::binary;
    if (overwrite) {
        mode |= std::ios::trunc;
    } else {
        mode |= std::ios::app;
    }

    m_zip_file.open(m_zip_path, mode);
    if (!m_zip_file.is_open()) {
        throw std::runtime_error("Failed to open zip file for writing");
    }
}

// 析构函数
ZipFile::~ZipFile() {
    if (!m_closed) {
        close();
    }
}

// 添加实体到zip归档
bool ZipFile::add_entity(ReadableFile& file) {
    if (m_closed) {
        return false;
    }

    // 获取文件元数据
    FileEntityMeta meta = file.get_meta();

    // 计算本地文件头的偏移量
    uint32_t local_header_offset = static_cast<uint32_t>(m_zip_file.tellp());

    // 写入本地文件头
    write_local_file_header(meta);

    // 写入文件内容并计算CRC32
    uint32_t crc32 = 0;
    uint32_t compressed_size = 0;

    // 使用ReadableFile的read(size)方法读取文件内容
    std::unique_ptr<std::vector<std::byte>> buffer;
    while (((buffer = file.read(4096))) && !buffer->empty()) {
        // 目前使用存储模式，不压缩
        m_zip_file.write(reinterpret_cast<const char*>(buffer->data()), buffer->size());
        crc32 = crc::crc32(reinterpret_cast<const char*>(buffer->data()), buffer->size());
        compressed_size += static_cast<uint32_t>(buffer->size());
    }

    // 写入中央目录记录
    write_central_directory_record(meta, local_header_offset, compressed_size, crc32);

    return true;
}

// 写入本地文件头
void ZipFile::write_local_file_header(const FileEntityMeta& meta) {
    ZipLocalFileHeader header;
    memset(&header, 0, sizeof(header));

    header.signature = ZipFileHeaderSignature::LocalFile;
    header.version_needed = ZipVersionNeeded::Version20;
    header.general_purpose = static_cast<uint16_t>(ZipGeneralPurposeBitFlag::Utf8Encoding);
    header.compression_method = ZipCompressionMethod::Store;

    // 设置时间戳（简化处理）
    header.last_mod_time = 0;
    header.last_mod_date = 0;

    // 文件大小（目前使用存储模式，压缩前后大小相同）
    header.uncompressed_size = static_cast<uint32_t>(meta.size);
    header.compressed_size = static_cast<uint32_t>(meta.size);

    // 文件名长度
    header.file_name_length = static_cast<uint16_t>(meta.path.string().size());
    header.extra_field_length = 0; // 暂不使用扩展字段

    // 写入头结构
    m_zip_file.write(reinterpret_cast<const char*>(&header), offsetof(ZipLocalFileHeader, file_name));

    // 写入文件名
    m_zip_file.write(meta.path.string().c_str(), meta.path.string().size());
}

// 写入中央目录记录
void ZipFile::write_central_directory_record(const FileEntityMeta& meta, uint32_t local_header_offset, uint32_t compressed_size, uint32_t crc32) {
    CentralDirectoryEntry entry;
    memset(&entry.record, 0, sizeof(entry.record));

    entry.record.signature = ZipFileHeaderSignature::CentralDirectoryFile;
    entry.record.version_made_by = ZipVersionMadeBy::Unix;
    entry.record.version_needed = ZipVersionNeeded::Version20;
    entry.record.general_purpose = static_cast<uint16_t>(ZipGeneralPurposeBitFlag::Utf8Encoding);
    entry.record.compression_method = ZipCompressionMethod::Store;

    // 设置时间戳（简化处理）
    entry.record.last_mod_time = 0;
    entry.record.last_mod_date = 0;

    entry.record.crc32 = crc32;
    entry.record.compressed_size = compressed_size;
    entry.record.uncompressed_size = static_cast<uint32_t>(meta.size);
    entry.record.file_name_length = static_cast<uint16_t>(meta.path.string().size());
    entry.record.extra_field_length = 0;
    entry.record.file_comment_length = 0;
    entry.record.disk_number_start = 0;

    // 设置外部文件属性（根据文件类型）
    if (meta.type == FileEntityType::RegularFile) {
        entry.record.external_file_attributes = 0x81A40000; // 普通文件
    } else if (meta.type == FileEntityType::Directory) {
        entry.record.external_file_attributes = 0x41ED0000; // 目录
    } else if (meta.type == FileEntityType::SymbolicLink) {
        entry.record.external_file_attributes = 0xA1FF0000; // 符号链接
    } else {
        entry.record.external_file_attributes = 0x81A40000; // 默认普通文件
    }

    entry.record.local_header_offset = local_header_offset;
    entry.file_name = meta.path.string();

    // 添加到中央目录
    m_central_directory.push_back(entry);
}

// 完成zip归档创建
void ZipFile::close() {
    if (m_closed) {
        return;
    }

    // 计算中央目录的偏移量和大小
    uint32_t central_directory_offset = static_cast<uint32_t>(m_zip_file.tellp());
    uint32_t central_directory_size = 0;

    // 写入中央目录
    for (const auto& entry : m_central_directory) {
        // 写入中央目录记录
        m_zip_file.write(reinterpret_cast<const char*>(&entry.record), offsetof(ZipCentralDirectoryFileHeader, file_name));

        // 写入文件名
        m_zip_file.write(entry.file_name.c_str(), entry.file_name.size());

        central_directory_size +=
            offsetof(ZipCentralDirectoryFileHeader, file_name) +
            static_cast<uint32_t>(entry.file_name.size());
    }

    // 写入中央目录结束标记
    write_end_of_central_directory(
        static_cast<uint32_t>(central_directory_offset),
        static_cast<uint32_t>(central_directory_size)
    );

    // 关闭文件
    m_zip_file.close();
    m_closed = true;
}

// 写入中央目录结束标记
void ZipFile::write_end_of_central_directory(uint32_t central_directory_offset, uint32_t central_directory_size) {
    ZipEndOfCentralDirectoryRecord eocd;
    memset(&eocd, 0, sizeof(eocd));

    eocd.signature = ZipFileHeaderSignature::EndOfCentralDirectory;
    eocd.disk_number = 0;
    eocd.central_directory_start_disk = 0;
    eocd.num_central_directory_records_on_this_disk = static_cast<uint16_t>(m_central_directory.size());
    eocd.total_central_directory_records = static_cast<uint16_t>(m_central_directory.size());
    eocd.central_directory_size = central_directory_size;
    eocd.central_directory_offset = central_directory_offset;
    eocd.comment_length = 0;

    m_zip_file.write(reinterpret_cast<const char*>(&eocd), offsetof(ZipEndOfCentralDirectoryRecord, comment));
}

// 实现：将FileEntityMeta转换为Zip本地文件头
zip::header::ZipLocalFileHeader ZipFile::convert_to_local_file_header(const FileEntityMeta& meta) {
    zip::header::ZipLocalFileHeader header;
    memset(&header, 0, sizeof(header));

    header.signature = ZipFileHeaderSignature::LocalFile;
    header.version_needed = ZipVersionNeeded::Version20;
    header.general_purpose = static_cast<uint16_t>(ZipGeneralPurposeBitFlag::Utf8Encoding);
    header.compression_method = ZipCompressionMethod::Store;

    // 设置时间戳（简化处理）
    header.last_mod_time = 0;
    header.last_mod_date = 0;

    // 文件大小（目前使用存储模式，压缩前后大小相同）
    header.uncompressed_size = static_cast<uint32_t>(meta.size);
    header.compressed_size = static_cast<uint32_t>(meta.size);

    // 文件名长度
    header.file_name_length = static_cast<uint16_t>(meta.path.string().size());
    header.extra_field_length = 0; // 暂不使用扩展字段

    return header;
}

// 实现：将FileEntityMeta转换为Zip中央目录记录
ZipFile::CentralDirectoryEntry ZipFile::convert_to_central_directory_record(
    const FileEntityMeta& meta,
    uint32_t local_header_offset,
    uint32_t compressed_size,
    uint32_t crc32) {
    ZipFile::CentralDirectoryEntry entry;
    memset(&entry.record, 0, sizeof(entry.record));

    entry.record.signature = ZipFileHeaderSignature::CentralDirectoryFile;
    entry.record.version_made_by = ZipVersionMadeBy::Unix;
    entry.record.version_needed = ZipVersionNeeded::Version20;
    entry.record.general_purpose = static_cast<uint16_t>(ZipGeneralPurposeBitFlag::Utf8Encoding);
    entry.record.compression_method = ZipCompressionMethod::Store;

    // 设置时间戳（简化处理）
    entry.record.last_mod_time = 0;
    entry.record.last_mod_date = 0;

    entry.record.crc32 = crc32;
    entry.record.compressed_size = compressed_size;
    entry.record.uncompressed_size = static_cast<uint32_t>(meta.size);
    entry.record.file_name_length = static_cast<uint16_t>(meta.path.string().size());
    entry.record.extra_field_length = 0;
    entry.record.file_comment_length = 0;
    entry.record.disk_number_start = 0;

    // 设置外部文件属性（根据文件类型）
    if (meta.type == FileEntityType::RegularFile) {
        entry.record.external_file_attributes = 0x81A40000; // 普通文件
    } else if (meta.type == FileEntityType::Directory) {
        entry.record.external_file_attributes = 0x41ED0000; // 目录
    } else if (meta.type == FileEntityType::SymbolicLink) {
        entry.record.external_file_attributes = 0xA1FF0000; // 符号链接
    } else {
        entry.record.external_file_attributes = 0x81A40000; // 默认普通文件
    }

    entry.record.local_header_offset = local_header_offset;
    entry.file_name = meta.path.string();

    return entry;
}
