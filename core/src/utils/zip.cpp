//
// Created by ycm on 2025/10/1.
//

#include "utils/zip.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <utility>

#include "encryption/rc.h"
#include "encryption/zip_crypto.h"
#include "utils/crc.h"
#include "utils/endian.h"

using namespace zip;
using namespace zip::header;

// 偏移常量（1601 到 1970 的 100ns 计数）
static constexpr uint64_t WINDOWS_TICK = 10000000;
static constexpr uint64_t SEC_TO_UNIX_EPOCH = 11644473600ULL;

static time_t dos_to_unix_time(const uint16_t dos_date, const uint16_t dos_time) {
    tm t{};

    // 清零以防受到本地时区/夏令时干扰
    t.tm_isdst = -1;

    // 解析时间
    t.tm_sec  = (dos_time & 0x1F) * 2;      // Bit 0-4
    t.tm_min  = (dos_time >> 5) & 0x3F;     // Bit 5-10
    t.tm_hour = (dos_time >> 11) & 0x1F;    // Bit 11-15

    // 解析日期
    t.tm_mday = (dos_date & 0x1F);          // Bit 0-4
    t.tm_mon  = ((dos_date >> 5) & 0x0F) - 1; // Bit 5-8 (tm_mon 是 0-11)
    t.tm_year = ((dos_date >> 9) & 0x7F) + 80; // Bit 9-15 (1980偏移, tm_year 是从 1900 开始)

    return mktime(&t); // 返回 Unix Timestamp
}
static std::pair<uint16_t, uint16_t> unix_time_to_dos(const time_t& time_point)
{
    tm tm{};
#ifdef _WIN32
    if (localtime_s(&tm, &time_point) != 0) {
        return {0, 0}; // 转换失败处理
    }
#else
    if (localtime_r(&time_point, &tm) == nullptr) {
        return {0, 0}; // 转换失败处理
    }
#endif
    const auto hour = tm.tm_hour & 0x1F,
    minute = tm.tm_min & 0x3F,
    second = (tm.tm_sec /2) & 0x1F,
    year = (tm.tm_year - 80) & 0x7F,
    month = (tm.tm_mon + 1) & 0x0F,
    day = tm.tm_mday & 0x1F;
    const uint16_t dos_time = (hour << 11) | (minute << 5) | second,
    dos_date = (year << 9) | (month << 5) | day;
    return {dos_date, dos_time};
}
static void eocd_le_to_host(ZipEndOfCentralDirectoryRecord& eocd)
{
    eocd.signature = le32toh(eocd.signature);
    eocd.disk_number = le16toh(eocd.disk_number);
    eocd.central_directory_start_disk = le16toh(eocd.central_directory_start_disk);
    eocd.num_central_directory_records_on_this_disk = le16toh(eocd.num_central_directory_records_on_this_disk);
    eocd.total_central_directory_records = le16toh(eocd.total_central_directory_records);
    eocd.central_directory_size = le32toh(eocd.central_directory_size);
    eocd.central_directory_offset = le32toh(eocd.central_directory_offset);
    eocd.comment_length = le16toh(eocd.comment_length);
}
static void eocd_host_to_le(ZipEndOfCentralDirectoryRecord& eocd)
{
    eocd.signature = htole32(eocd.signature);
    eocd.disk_number = htole16(eocd.disk_number);
    eocd.central_directory_start_disk = htole16(eocd.central_directory_start_disk);
    eocd.num_central_directory_records_on_this_disk = htole16(eocd.num_central_directory_records_on_this_disk);
    eocd.total_central_directory_records = htole16(eocd.total_central_directory_records);
    eocd.central_directory_size = htole32(eocd.central_directory_size);
    eocd.central_directory_offset = htole32(eocd.central_directory_offset);
    eocd.comment_length = htole16(eocd.comment_length);
}
static void lfh_le_to_host(ZipLocalFileHeader& lfh)
{
    lfh.signature = le32toh(lfh.signature);
    lfh.version_needed = le16toh(lfh.version_needed);
    lfh.general_purpose = le16toh(lfh.general_purpose);
    lfh.compression_method = le16toh(lfh.compression_method);
    lfh.last_mod_time = le16toh(lfh.last_mod_time);
    lfh.last_mod_date = le16toh(lfh.last_mod_date);
    lfh.crc32 = le32toh(lfh.crc32);
    lfh.compressed_size = le32toh(lfh.compressed_size);
    lfh.uncompressed_size = le32toh(lfh.uncompressed_size);
    lfh.file_name_length = le16toh(lfh.file_name_length);
    lfh.extra_field_length = le16toh(lfh.extra_field_length);
}
static void lfh_host_to_le(ZipLocalFileHeader& lfh)
{
    lfh.signature = htole32(lfh.signature);
    lfh.version_needed = htole16(lfh.version_needed);
    lfh.general_purpose = htole16(lfh.general_purpose);
    lfh.compression_method = htole16(lfh.compression_method);
    lfh.last_mod_time = htole16(lfh.last_mod_time);
    lfh.last_mod_date = htole16(lfh.last_mod_date);
    lfh.crc32 = htole32(lfh.crc32);
    lfh.compressed_size = htole32(lfh.compressed_size);
    lfh.uncompressed_size = htole32(lfh.uncompressed_size);
    lfh.file_name_length = htole16(lfh.file_name_length);
    lfh.extra_field_length = htole16(lfh.extra_field_length);
}
static void cdfh_le_to_host(ZipCentralDirectoryFileHeader& cdfh)
{
    cdfh.signature = le32toh(cdfh.signature);
    // generator_version is a single byte, no need to convert
    // version_make_by is a single byte, no need to convert
    cdfh.version_needed = le16toh(cdfh.version_needed);
    cdfh.general_purpose = le16toh(cdfh.general_purpose);
    cdfh.compression_method = le16toh(cdfh.compression_method);
    cdfh.last_mod_time = le16toh(cdfh.last_mod_time);
    cdfh.last_mod_date = le16toh(cdfh.last_mod_date);
    cdfh.crc32 = le32toh(cdfh.crc32);
    cdfh.compressed_size = le32toh(cdfh.compressed_size);
    cdfh.uncompressed_size = le32toh(cdfh.uncompressed_size);
    cdfh.file_name_length = le16toh(cdfh.file_name_length);
    cdfh.extra_field_length = le16toh(cdfh.extra_field_length);
    cdfh.file_comment_length = le16toh(cdfh.file_comment_length);
    cdfh.disk_number_start = le16toh(cdfh.disk_number_start);
    cdfh.internal_file_attributes = le16toh(cdfh.internal_file_attributes);
    cdfh.external_file_attributes = le32toh(cdfh.external_file_attributes);
    cdfh.local_header_offset = le32toh(cdfh.local_header_offset);
}
static void cdfh_host_to_le(ZipCentralDirectoryFileHeader& cdfh)
{
    cdfh.signature = htole32(cdfh.signature);
    // generator_version is a single byte, no need to convert
    // version_make_by is a single byte, no need to convert
    cdfh.version_needed = htole16(cdfh.version_needed);
    cdfh.general_purpose = htole16(cdfh.general_purpose);
    cdfh.compression_method = htole16(cdfh.compression_method);
    cdfh.last_mod_time = htole16(cdfh.last_mod_time);
    cdfh.last_mod_date = htole16(cdfh.last_mod_date);
    cdfh.crc32 = htole32(cdfh.crc32);
    cdfh.compressed_size = htole32(cdfh.compressed_size);
    cdfh.uncompressed_size = htole32(cdfh.uncompressed_size);
    cdfh.file_name_length = htole16(cdfh.file_name_length);
    cdfh.extra_field_length = htole16(cdfh.extra_field_length);
    cdfh.file_comment_length = htole16(cdfh.file_comment_length);
    cdfh.disk_number_start = htole16(cdfh.disk_number_start);
    cdfh.internal_file_attributes = htole16(cdfh.internal_file_attributes);
    cdfh.external_file_attributes = htole32(cdfh.external_file_attributes);
    cdfh.local_header_offset = htole32(cdfh.local_header_offset);
}
static std::chrono::system_clock::time_point ntfs_u64_to_time_point(const uint64_t ntfs_time)
{
    // 1. 先减去 1601 到 1970 的偏移量（以 100ns 为单位）
    const uint64_t unix_100ns = ntfs_time - (SEC_TO_UNIX_EPOCH * WINDOWS_TICK);

    // 2. 转为纳秒 (100ns * 100 = 1ns)
    const auto duration_ns = std::chrono::nanoseconds(unix_100ns * 100);

    // 3. 构造 time_point
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(duration_ns)
    );
}
static uint64_t time_point_to_ntfs_u64(const std::chrono::system_clock::time_point& time_point)
{
    // 1. 获取自 Unix 纪元以来的持续时间（以纳秒为单位）
    const auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        time_point.time_since_epoch()
    ).count();

    // 2. 转为 100ns 单位 (1ns / 100 = 100ns)
    const uint64_t duration_100ns = duration_ns / 100;

    // 3. 加上 1601 到 1970 的偏移量（以 100ns 为单位）
    return duration_100ns + (SEC_TO_UNIX_EPOCH * WINDOWS_TICK);
}
static std::chrono::system_clock::time_point unix_u32_to_time_point(const uint32_t time_point)
{
    return std::chrono::system_clock::time_point(std::chrono::seconds(time_point));
}
static uint32_t time_point_to_unix_u32(const std::chrono::system_clock::time_point& time_point)
{
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            time_point.time_since_epoch()
        ).count()
    );
}
// 将SQLZipEntity转换为FileEntityMeta
static FileEntityMeta sql_zip_entity2file_meta(const db::ZipInitializationStrategy::SQLZipEntity& entity)
{
    auto [version_made_by, version_needed, general_purpose, compression_method, last_modified, crc32, compressed_size, uncompressed_size, disk_number, internal_attributes, external_attributes, local_header_offset, filename, extra_field, file_comment] = entity;

    // 根据外部属性判断文件类型
    auto type = FileEntityType::RegularFile;
    if (external_attributes & 0x40000000) { // S_IFDIR
        type = FileEntityType::Directory;
    } else if (external_attributes & 0xA0000000) { // S_IFLNK
        type = FileEntityType::SymbolicLink;
    }

    // 创建FileEntityMeta
    FileEntityMeta meta = {
        std::filesystem::path(filename),
        type,
        static_cast<size_t>(uncompressed_size),
        std::chrono::system_clock::from_time_t(last_modified),
        std::chrono::system_clock::from_time_t(last_modified),
        std::chrono::system_clock::from_time_t(last_modified),
        0, // posix_mode
        0, // uid
        0, // gid
        "", // user_name
        "", // group_name
        static_cast<uint32_t>(external_attributes),
        "", // symbolic_link_target
        0, // device_major
        0  // device_minor
    };

    return meta;
}

// 析构函数
ZipFile::~ZipFile() {
    if (is_valid_) {
        close();
    }
}
void ZipFile::init_db_from_zip()
{
    if (!is_valid_ || !ifs_ || !ifs_->is_open())
    {
        return;
    }

    // 获取文件大小
    ifs_->seekg(0, std::ios::end);
    const size_t file_size = ifs_->tellg();

    // 查找中央目录结束记录 (End of Central Directory Record)
    // EOCD记录通常在文件末尾，范围在文件最后22字节到文件最后65557字节之间

    // 搜索EOCD签名
    const ZipEndOfCentralDirectoryRecord *eocd_p = nullptr;
    char buffer[2048];
    size_t offset_from_end = 0;
    size_t start_of_buffer = 0;
    if (2048 > file_size)
    {
        start_of_buffer = 2048 - file_size;
        ifs_->seekg(0, std::ios::beg);
    } else
    {
        ifs_->seekg(-2048, std::ios::end);
    }
    ifs_->read(buffer + start_of_buffer, static_cast<long long>(2048 - start_of_buffer));
    if (start_of_buffer)
    {
        ifs_->seekg(0, std::ios::beg);
    } else
    {
        ifs_->seekg(-2048, std::ios::cur);
    }
    while (!start_of_buffer && offset_from_end + 2048 < file_size)
    {
        size_t i;
        for (i = 0; i <= 2048 - sizeof(ZipEndOfCentralDirectoryRecord); i++)
        {
            eocd_p = reinterpret_cast<ZipEndOfCentralDirectoryRecord*>(buffer + i);
            if (le32toh(eocd_p->signature) == ZipFileHeaderSignature::EndOfCentralDirectory)
            {
                if (const auto real_comment_length =
                        offset_from_end + 2048 - i - sizeof(ZipEndOfCentralDirectoryRecord);
                    real_comment_length != le16toh(eocd_p->comment_length))
                {
                    std::cerr << "no EOCD, exit!" << std::endl;
                    is_valid_ = false;
                    return;
                }
                break;
            }
        }
        if (i <= 2048 - sizeof(ZipEndOfCentralDirectoryRecord)) break;
        eocd_p = nullptr;
        memcpy_s(buffer+1024, 1024, buffer+1024, 1024);
        offset_from_end += 1024;
        if (auto next_read = file_size - offset_from_end - 1024; next_read > 1024)
        {
            next_read = 1024;
            ifs_->seekg(-1024, std::ios::cur);
            ifs_->read(buffer, 1024);
            if (ifs_->gcount() != next_read)
            {
                std::cerr << "no EOCD, exit!" << std::endl;
                is_valid_ = false;
                return;
            }
        } else
        {
            ifs_->seekg(static_cast<long long>(-next_read), std::ios::cur);
            ifs_->read(buffer, static_cast<long long>(next_read));
            if (ifs_->gcount() != next_read)
            {
                std::cerr << "no EOCD, exit!" << std::endl;
                is_valid_ = false;
                return;
            }
            start_of_buffer = 1024 - next_read;
        }
    }
    // 处理读到文件头的情况
    if (!eocd_p)
    {
        size_t i;
        for (i = start_of_buffer; i <= 2048 - sizeof(ZipEndOfCentralDirectoryRecord); i++)
        {
            eocd_p = reinterpret_cast<ZipEndOfCentralDirectoryRecord*>(buffer + i);
            if (le32toh(eocd_p->signature) == ZipFileHeaderSignature::EndOfCentralDirectory)
            {
                if (const auto real_comment_length =
                        offset_from_end + 2048 - i - sizeof(ZipEndOfCentralDirectoryRecord);
                    real_comment_length != le16toh(eocd_p->comment_length))
                {
                    std::cerr << "no EOCD, exit!" << std::endl;
                    is_valid_ = false;
                    return;
                }
                break;
            }
        }
        if (i > 2048 - sizeof(ZipEndOfCentralDirectoryRecord))
        {
            std::cerr << "no EOCD, exit!" << std::endl;
            is_valid_ = false;
            return;
        }
    }
    // 拷贝eocd,避免受 buffer 影响
    eocd_ = *eocd_p;
    // 从小端序中转换
    eocd_le_to_host(eocd_);

    if (eocd_.comment_length)
    {
        ifs_->seekg(-eocd_.comment_length, std::ios::end);
        comment_.resize(eocd_.comment_length);
        ifs_->read(&comment_[0], eocd_.comment_length);
        comment_.resize(ifs_->gcount());
    }
    // 解析中央目录
    ifs_->seekg(eocd_.central_directory_offset, std::ios::beg);
    if (!ifs_->good())
    {
        std::cerr << "ifs not good" << std::endl;
        is_valid_ = false;
        return;
    }
    for (size_t i=0; i < eocd_.total_central_directory_records; i++)
    {
        ZipCentralDirectoryFileHeader cdfh;
        memset(reinterpret_cast<char*>(&cdfh), 0, sizeof(cdfh));
        ifs_->read(reinterpret_cast<char*>(&cdfh), sizeof(cdfh));
        if (ifs_->gcount() != sizeof(cdfh) || le32toh(cdfh.signature) != ZipFileHeaderSignature::CentralDirectoryFile)
        {
            std::cerr << "invalid CDFH signature '<< " << std::uppercase << std::setw(8) << std::setfill('0') << std::hex << static_cast<uint16_t>(cdfh.signature) << " <<', not '" << static_cast<uint16_t>(ZipFileHeaderSignature::CentralDirectoryFile) << "', exit!" << std::endl;
            is_valid_ = false;
            return;
        }
        // 从小端序中转换
        cdfh_le_to_host(cdfh);
        std::string filename, file_comment;
        std::vector<uint8_t> extra_field;
        if (cdfh.file_name_length)
        {
            filename.resize(cdfh.file_name_length);
            ifs_->read(&filename[0], cdfh.file_name_length);
        }
        if (cdfh.extra_field_length)
        {
            extra_field.resize(cdfh.extra_field_length);
            ifs_->read(reinterpret_cast<char*>(extra_field.data()), cdfh.extra_field_length);
        }
        if (cdfh.file_comment_length)
        {
            file_comment.resize(cdfh.file_comment_length);
            ifs_->read(&file_comment[0], cdfh.file_comment_length);
        }
        if (!insert_entity(&cdfh, filename, extra_field, file_comment))
        {
            std::cerr << "insert file '" << filename << "' failed, exit!" << std::endl;
            is_valid_ = false;
            return;
        }
    }
}
bool ZipFile::insert_entity(const ZipCentralDirectoryFileHeader* cdfh, const std::string& file_name,
                            const std::vector<uint8_t>& extra_field, const std::string& file_comment) const
{
    if (cdfh == nullptr) return false;
    const auto stmt = db_.create_statement(
        "INSERT INTO zip_entity (" + db::ZipInitializationStrategy::SQLEntityColumns + ") "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);"
    );
    if (!stmt) return false;
    int i = 1;
    db::bind_parameter(stmt.get(), i++, static_cast<uint16_t>(cdfh->version_made_by));
    db::bind_parameter(stmt.get(), i++, static_cast<uint16_t>(cdfh->version_needed));
    db::bind_parameter(stmt.get(), i++, cdfh->general_purpose);
    db::bind_parameter(stmt.get(), i++, static_cast<uint16_t>(cdfh->compression_method));
    db::bind_parameter(stmt.get(), i++, dos_to_unix_time(cdfh->last_mod_date, cdfh->last_mod_time));
    db::bind_parameter(stmt.get(), i++, cdfh->crc32);
    db::bind_parameter(stmt.get(), i++, cdfh->compressed_size);
    db::bind_parameter(stmt.get(), i++, cdfh->uncompressed_size);
    db::bind_parameter(stmt.get(), i++, cdfh->disk_number_start);
    db::bind_parameter(stmt.get(), i++, cdfh->internal_file_attributes);
    db::bind_parameter(stmt.get(), i++, cdfh->external_file_attributes);
    db::bind_parameter(stmt.get(), i++, cdfh->local_header_offset);
    db::bind_parameter(stmt.get(), i++, file_name);
    if (extra_field.empty())
    {
        db::bind_parameter_null(stmt.get(), i++);
    } else
    {
        db::bind_parameter(stmt.get(), i++, extra_field);
    }
    db::bind_parameter(stmt.get(), i++, file_comment);

    return db_.execute(*stmt, true);
}

// 添加实体到zip归档
bool ZipFile::add_entity(ReadableFile& file, ZipCompressionMethod compression_method, ZipEncryptionMethod encryption_method) {
    if (!is_valid_) {
        return false;
    }
    std::random_device rd;
    std::mt19937 rand(rd());

    // 获取文件元数据
    auto meta = file.get_meta();
    update_file_entity_meta(meta);
    std::string filename = meta.path.generic_u8string();
    std::vector<uint8_t> extra_field;

    // 设置外部文件属性和扩展字段
    ZipVersionMadeBy version_made_by = meta.type == FileEntityType::SymbolicLink ? ZipVersionMadeBy::Unix : SYSTEM_VERSION_MADE_BY;
    uint32_t external_file_attributes = 0;
    // ReSharper disable once CppDFAConstantConditions
    if (version_made_by == ZipVersionMadeBy::MS_DOS || version_made_by == ZipVersionMadeBy::NTFS)
    {
        external_file_attributes = meta.windows_attributes;
    }
    // ReSharper disable once CppDFAConstantConditions
    else if (version_made_by == ZipVersionMadeBy::Unix)
    {
        if (meta.posix_mode)
        {
            external_file_attributes = (meta.posix_mode << 16);
        } else
        {
            // 如果没有提供posix_mode，则根据文件类型设置默认权限
            switch (meta.type)
            {
                case FileEntityType::RegularFile:
                    external_file_attributes = 0x81A40000; // 普通文件，rw-r--r--
                    break;
                case FileEntityType::Directory:
                    external_file_attributes = 0x41ED0000; // 目录，r
                    break;
                case FileEntityType::SymbolicLink:
                    external_file_attributes = 0xA1FF0000; // 符号链接，rwxrwxrwx
                    break;
                default:
                    external_file_attributes = 0x81A40000; // 默认普通文件，rw-r--r--
                    break;
            }
            // if (meta.type == FileEntityType::RegularFile)
            // {
            //     external_file_attributes = 0x81A40000; // 普通文件
            // } else if (meta.type == FileEntityType::Directory)
            // {
            //     external_file_attributes = 0x41ED0000; // 目录
            // } else if (meta.type == FileEntityType::SymbolicLink)
            // {
            //     external_file_attributes = 0xA1FF0000; // 符号链接
            // } else
            // {
            //     external_file_attributes = 0x81A40000; // 默认普通文件
            // }
        }
    }
    else
    {
        std::cerr << "unsupported version made by: " << static_cast<uint16_t>(version_made_by) << "\n";
        return false;
    }
    if (meta.type == FileEntityType::Directory)
    {
        external_file_attributes |= 0x10; // 设置目录标志
    }

    // 写扩展字段
    // 7-zip 虽然设置的 version_make_by 为 MS_DOS,但依然写了NTFS的扩展字段
    // ReSharper disable once CppDFAConstantConditions
    if (version_made_by == ZipVersionMadeBy::NTFS || version_made_by == ZipVersionMadeBy::MS_DOS)
    {
        const auto ntfs_extra_field = ntfs2extra_field(
            time_point_to_ntfs_u64(meta.modification_time),
            time_point_to_ntfs_u64(meta.access_time),
            time_point_to_ntfs_u64(meta.creation_time)
        );
        extra_field.insert(extra_field.end(), ntfs_extra_field.begin(), ntfs_extra_field.end());
    }
    // ReSharper disable once CppDFAConstantConditions
    else if (version_made_by == ZipVersionMadeBy::Unix)
    {
        const auto unix_extra_field = unix2extra_field(
            time_point_to_unix_u32(meta.access_time),
            time_point_to_unix_u32(meta.modification_time),
            meta.uid,
            meta.gid
        );
        extra_field.insert(extra_field.end(), unix_extra_field.begin(), unix_extra_field.end());
    }

    // 计算本地文件头的偏移量
    const uint32_t local_header_offset = ofs_->tellp();

    // 写入本地文件头
    ZipLocalFileHeader local_header { (ZipFileHeaderSignature::LocalFile), (ZipVersionNeeded::Version20) };
    // 设置版本需要
    // 2.0 - File is encrypted using traditional PKWARE encryption
    if (encryption_method == ZipEncryptionMethod::ZipCrypto)
    {
        if (static_cast<uint16_t>(local_header.version_needed) < static_cast<uint16_t>(ZipVersionNeeded::Version20))
        {
            local_header.version_needed = ZipVersionNeeded::Version20;
        }
    }
    // 5.0 - File is encrypted using RC4 encryption
    if (encryption_method == ZipEncryptionMethod::RC4)
    {
        if (static_cast<uint16_t>(local_header.version_needed) < static_cast<uint16_t>(ZipVersionNeeded::Version50))
        {
            local_header.version_needed = ZipVersionNeeded::Version50;
        }
    }

    local_header.general_purpose = (static_cast<uint16_t>(ZipGeneralPurposeBitFlag::Utf8Encoding));
    local_header.compression_method = (compression_method);
    // 设置时间戳
    auto [dos_date, dos_time] = unix_time_to_dos(std::chrono::duration_cast<std::chrono::seconds>(
        meta.modification_time.time_since_epoch()).count());
    local_header.last_mod_date = (dos_date);
    local_header.last_mod_time = (dos_time);
    // crc32
    local_header.crc32 = (0); // 先写0，后面计算完再更新
    // 文件大小（目前使用存储模式，压缩前后大小相同）
    local_header.uncompressed_size = (static_cast<uint32_t>(meta.size));
    local_header.compressed_size = (static_cast<uint32_t>(meta.size));
    // 文件名长度
    local_header.file_name_length = (static_cast<uint16_t>(filename.size()));
    // 扩展字段长度
    local_header.extra_field_length = extra_field.size();

    // 设置加密相关信息
    if (encryption_method == ZipEncryptionMethod::ZipCrypto)
    {
        // 开头写入12字节的头
        local_header.compressed_size += 12;
        local_header.general_purpose |= static_cast<uint16_t>(ZipGeneralPurposeBitFlag::Encrypted);
        local_header.general_purpose &= ~static_cast<uint16_t>(ZipGeneralPurposeBitFlag::StrongEncryption);
    }
    else if (encryption_method == ZipEncryptionMethod::RC4)
    {
        local_header.general_purpose |= static_cast<uint16_t>(ZipGeneralPurposeBitFlag::Encrypted);
        local_header.general_purpose |= static_cast<uint16_t>(ZipGeneralPurposeBitFlag::StrongEncryption);
    }

    lfh_host_to_le(local_header);
    ofs_->write(reinterpret_cast<const char*>(&local_header), sizeof(local_header));

    // 写入文件头后立刻紧跟文件名
    // ReSharper disable once CppRedundantCastExpression
    ofs_->write(reinterpret_cast<const char*>(filename.c_str()), static_cast<long long>(filename.size()));
    // 写入文件名后紧跟扩展字段
    if (!extra_field.empty())
    {
        ofs_->write(reinterpret_cast<const char*>(extra_field.data()), static_cast<long long>(extra_field.size()));
    }

    // 写入文件内容并计算CRC32
    uint32_t crc32 = 0;
    crc::CRC32 crc32_inst;
    uint32_t compressed_size = 0;
    encryption::ZipCrypto zip_crypto(password_);
    encryption::RC4 rc4_encryptor{};

    if (meta.type == FileEntityType::RegularFile)
    {
        if (encryption_method == ZipEncryptionMethod::ZipCrypto)
        {
            std::uniform_int_distribution<uint16_t> dist(0, 255);
            // 避免二次写入,这里直接用 last_mod_time 字段写入头
            for (int i=0; i<11; i++)
            {
                uint8_t bit = zip_crypto.encrypt(dist(rand));
                ofs_->write(reinterpret_cast<const char*>(&bit), 1);
            }
            uint8_t last_bit = zip_crypto.encrypt((local_header.last_mod_time>>8) & 0xff);
            ofs_->write(reinterpret_cast<const char*>(&last_bit), 1);
            compressed_size += 12;
            zip_crypto = encryption::ZipCrypto(password_);
        } else if (encryption_method == ZipEncryptionMethod::RC4)
        {
            std::vector rc4_pwd{password_};
            if (rc4_pwd.size() < 32)
            {
                rc4_pwd.resize(32, 0);
            } else if (rc4_pwd.size() > 448)
            {
                rc4_pwd.resize(448);
            }
            rc4_encryptor = encryption::RC4(rc4_pwd);
            encryption::RC4 tmp_rc4_encryptor {rc4_pwd};

            DecryptionHeaderRecord rc4_dec_header {
                {},
                3,
                ZipEncryptionMethod::RC4,
                tmp_rc4_encryptor.bit_len(),
                1,
                {}, {}, std::vector<uint8_t>(16)
            };
            std::uniform_int_distribution<uint16_t> dist(0, 255);
            crc::CRC32 tmp_crc32_inst;
            for (unsigned char & i : rc4_dec_header.v_data)
            {
                uint8_t bit = (dist(rand));
                tmp_crc32_inst.update(tmp_rc4_encryptor.encrypt(bit));
                i = bit;
            }
            rc4_dec_header.v_crc32 = tmp_crc32_inst.finalize();
            const auto rc4_header_bytes = make_decryption_header(rc4_dec_header);
            ofs_->write(reinterpret_cast<const char*>(rc4_header_bytes.data()), static_cast<long long>(rc4_header_bytes.size()));
            compressed_size += static_cast<uint32_t>(rc4_header_bytes.size());
        }
    }

    // 使用ReadableFile的read(size)方法读取文件内容
    if (compression_method == ZipCompressionMethod::Store)
    {
        std::unique_ptr<std::vector<std::byte>> buffer;
        while (((buffer = file.read(4096))) && buffer && !buffer->empty()) {
            crc32_inst.update(buffer->data(), buffer->size());
            compressed_size += static_cast<uint32_t>(buffer->size());
            // 目前使用存储模式，不压缩
            if (encryption_method == ZipEncryptionMethod::ZipCrypto)
            {
                for (const auto c : *buffer)
                {
                    uint8_t bit = zip_crypto.encrypt(static_cast<uint8_t>(c));
                    ofs_->write(reinterpret_cast<const char*>(&bit), 1);
                }
            } else if (encryption_method == ZipEncryptionMethod::RC4)
            {
                for (const auto c : *buffer)
                {
                    uint8_t bit = rc4_encryptor.encrypt(static_cast<uint8_t>(c));
                    ofs_->write(reinterpret_cast<const char*>(&bit), 1);
                }
            }
            else
            {
                ofs_->write(reinterpret_cast<const char*>(buffer->data()), static_cast<long long>(buffer->size()));
            }
        }
    } else
    {
        std::cerr << "unsupported compression method: " << static_cast<uint16_t>(compression_method) << "\n";
        return false;
    }

    // 回写CRC32和压缩大小到本地文件头
    crc32 = crc32_inst.finalize();
    const auto file_end_pos = ofs_->tellp();
    ofs_->seekp(static_cast<long long>(local_header_offset + offsetof(ZipLocalFileHeader, crc32)));
    crc32 = htole32(crc32);
    ofs_->write(reinterpret_cast<const char*>(&crc32), sizeof(crc32));
    crc32 = le32toh(crc32);
    compressed_size = htole32(compressed_size);
    ofs_->write(reinterpret_cast<const char*>(&compressed_size), sizeof(compressed_size));
    compressed_size = htole32(compressed_size);
    ofs_->seekp(file_end_pos);

    // Extra Field 0x0017 in central header only.
    // see https://pkwaredownloads.blob.core.windows.net/pkware-general/Documentation/APPNOTE-6.3.9.TXT
    // Only deal with encryption method we support here.
    if (encryption_method == ZipEncryptionMethod::RC4)
    {
        std::vector<uint8_t> rc4_extra_field;
        rc4_extra_field.resize(12, 0);
        *reinterpret_cast<uint16_t*>(&rc4_extra_field[0]) = htole16(0x0017);
        *reinterpret_cast<uint16_t*>(&rc4_extra_field[2]) = htole16(8);
        *reinterpret_cast<uint16_t*>(&rc4_extra_field[4]) = htole16(2); // format = 2
        *reinterpret_cast<uint16_t*>(&rc4_extra_field[6]) = htole16(static_cast<uint16_t>(ZipEncryptionMethod::RC4));
        *reinterpret_cast<uint16_t*>(&rc4_extra_field[8]) = htole16(rc4_encryptor.bit_len());
        *reinterpret_cast<uint16_t*>(&rc4_extra_field[10]) = htole16(1); // flags
        extra_field.insert(extra_field.end(), rc4_extra_field.begin(), rc4_extra_field.end());
    }

    // 将文件元数据插入到数据库中
    // 这里不涉及到大小端序问题，因为我们存储的是原始值
    ZipCentralDirectoryFileHeader central_directory_header
    {
        ZipFileHeaderSignature::CentralDirectoryFile,
        static_cast<uint8_t>(version_make_by_),
        // 如果是符号链接,为了兼容性强制标记为Unix系统
        version_made_by,
        le16toh(local_header.version_needed),
        le16toh(local_header.general_purpose),
        le16toh(compression_method),
        dos_time,
        dos_date,
        crc32,
        compressed_size,
        static_cast<uint32_t>(meta.size),
        le16toh(local_header.file_name_length),
        le16toh(static_cast<uint16_t>(extra_field.size())),
        0,  // 暂时不写入文件注释
        0,  // 不使用分卷压缩
        // 内部文件属性
        0,
        external_file_attributes,
        local_header_offset
    };
    if (!insert_entity(&central_directory_header, filename, extra_field, ""))
    {
        return false;
    }
    return true;
}

ZipFile::CentralDirectoryEntry ZipFile::sql_entity_to_cdfh(const db::ZipInitializationStrategy::SQLZipEntity& entity)
{
    auto [version_made_by, version_needed, general_purpose, compression_method, last_modified, crc32, compressed_size, uncompressed_size, disk_number, internal_attributes, external_attributes, local_header_offset, filename, extra_field, file_comment] = entity;
    const auto [last_mod_date, last_mod_time] = unix_time_to_dos(static_cast<time_t>(last_modified));
    const CentralDirectoryEntry entry = {
        filename,
        extra_field,
        file_comment,
        {
            ZipFileHeaderSignature::CentralDirectoryFile,
            0,
            static_cast<ZipVersionMadeBy>(version_made_by),
            static_cast<ZipVersionNeeded>(version_needed),
            general_purpose,
            static_cast<ZipCompressionMethod>(compression_method),
            last_mod_time,
            last_mod_date,
            crc32,
            compressed_size,
            uncompressed_size,
            static_cast<uint16_t>(filename.size()),
            static_cast<uint16_t>(extra_field.size()),
            static_cast<uint16_t>(file_comment.size()),
            disk_number,
            internal_attributes,
            external_attributes,
            local_header_offset
        }
    };
    return entry;
}

// 实现list_dir方法
std::vector<ZipFile::CentralDirectoryEntry> ZipFile::list_dir(const std::filesystem::path& path) const
{
    // path like "/...", and not contain any driver letter
    if (path.has_root_name() || !path.is_relative())
        return {}; // cannot analyze a path starts with "C:\"

    // 调试输出：检查数据库是否已初始化
    if (!db_.is_open()) {
        return {};
    }

    auto stmt = db_.create_statement(
        "SELECT " + db::ZipInitializationStrategy::SQLEntityColumns +
        " FROM zip_entity WHERE filename LIKE ? ORDER BY filename ASC;"
    );

    std::string like_path = path.generic_u8string();
    if (like_path == "." || like_path.empty())
        like_path = "/%";
    else if (like_path.back() == '/')
        like_path += "%";
    else
        like_path += "/%";
    if (like_path.front() == '/')
        like_path = like_path.substr(1);

    sqlite3_bind_text(stmt.get(), 1, like_path.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<ZipFile::CentralDirectoryEntry> results;
    auto rs = db_.query<db::ZipInitializationStrategy::SQLZipEntity>(std::move(stmt));
    for (const auto& entity : rs)
    {
        results.push_back(sql_entity_to_cdfh(entity));
    }
    return results;
}

// 实现get_file_stream方法
std::unique_ptr<ZipFile::ZipIstream> ZipFile::get_file_stream(const std::filesystem::path& path)
{
    if (!is_valid_ || !ifs_ || !ifs_->is_open())
    {
        return nullptr;
    }
    // path like "/...", and not contain any driver letter
    if (path.has_root_name() || !path.is_relative())
        return nullptr; // cannot analyze a path starts with "C:\"

    auto path_str = path.generic_u8string();
    if (path_str.empty())
        return nullptr;
    while (!path_str.empty() && path_str.front() == '.')
    {
        path_str = path_str.substr(1);
    }
    if (path_str.empty()) return nullptr;
    while (!path_str.empty() && path_str.front() == '/')
    {
        path_str = path_str.substr(1);
    }
    if (path_str.empty()) return nullptr;

    auto stmt = db_.create_statement(
        "SELECT " + db::ZipInitializationStrategy::SQLEntityColumns +
        " FROM zip_entity WHERE filename = ? LIMIT 1;"
    );

    // reinterpret_cast for C++20
    sqlite3_bind_text(stmt.get(), 1, reinterpret_cast<const char*>(path_str.c_str()), -1, SQLITE_TRANSIENT);

    std::unique_ptr<ZipIstream> zip_stream;
    db::ZipInitializationStrategy::SQLZipEntity entity;
    CentralDirectoryEntry cdfh;
    try
    {
        entity = db_.query_one<db::ZipInitializationStrategy::SQLZipEntity>(std::move(stmt));
        cdfh = sql_entity_to_cdfh(entity);
    } catch ([[maybe_unused]] const std::exception& e)
    {
        return nullptr;
    }
    ZipLocalFileHeader lfh;
    ifs_->seekg(cdfh.record.local_header_offset, std::ios::beg);
    ifs_->read(reinterpret_cast<char*>(&lfh), sizeof(ZipLocalFileHeader));
    if (ifs_->gcount() != sizeof(ZipLocalFileHeader) || le32toh(lfh.signature) != ZipFileHeaderSignature::LocalFile)
    {
        return nullptr;
    }
    // 从小端序中转换
    lfh_le_to_host(lfh);
    size_t real_offset = cdfh.record.local_header_offset + sizeof(lfh) + lfh.file_name_length + lfh.extra_field_length;
    auto meta = sql_zip_entity2file_meta(entity);
    std::unique_ptr<ZipIstreamBuf> stream_buf = nullptr;

    if ((lfh.general_purpose & static_cast<uint16_t>(ZipGeneralPurposeBitFlag::Encrypted)) && meta.size > 0)
    {
        // ReSharper disable once CppDFAUnreachableCode
        do
        {
            auto encryption_method = ZipEncryptionMethod::Unknown;
            if (lfh.general_purpose & static_cast<uint16_t>(ZipGeneralPurposeBitFlag::StrongEncryption))
            {
                const auto extra_field_list = get_extra_field_list(cdfh.extra_field);
                for (const auto& field: extra_field_list)
                {
                    encryption_method = get_encryption_method(field).alg_id;
                    if (encryption_method != ZipEncryptionMethod::Unknown)
                    {
                        break;
                    }
                }
            } else
            {
                encryption_method = ZipEncryptionMethod::ZipCrypto;
            }
            if (encryption_method == ZipEncryptionMethod::Unknown && cdfh.record.compression_method == ZipCompressionMethod::AES_Encryption) {
                encryption_method = ZipEncryptionMethod::AES256;
            }

            if (encryption_method == ZipEncryptionMethod::ZipCrypto)
            {
                // 解密头12个字节,判断是否与 CRC 相符
                encryption::ZipCrypto decoder{password_};
                uint8_t zip_crypto_header[12];
                ifs_->seekg(static_cast<long long>(real_offset), std::ios::beg);
                ifs_->read(reinterpret_cast<char*>(zip_crypto_header), 12);
                for (unsigned char & i : zip_crypto_header)
                {
                    i = decoder.decrypt(i);
                }
                //  After the header is decrypted, the last 1 or 2 bytes in Buffer
                // SHOULD be the high-order word/byte of the CRC for the file being
                // decrypted, stored in Intel low-byte/high-byte order.  Versions of
                // PKZIP prior to 2.0 used a 2 byte CRC check; a 1 byte CRC check is
                // used on versions after 2.0.
                // see https://pkwaredownloads.blob.core.windows.net/pkware-general/Documentation/APPNOTE-6.3.9.TXT
                // check highest bit of CRC
                if (static_cast<uint8_t>((lfh.crc32 >> 24) & 0xFF) == zip_crypto_header[11])
                {
                    if (auto version_needed = static_cast<uint16_t>(lfh.version_needed);
                        version_needed >= 20 && version_needed < 30)
                    {
                        if (static_cast<uint8_t>((lfh.crc32 >> 16) & 0xFF) == zip_crypto_header[10])
                        {
                            stream_buf = std::make_unique<ZipCryptoIstreamBuf>(*ifs_.get(), decoder, real_offset + 12, meta.size);  // meta.size 使用的是 uncompression_size
                            break;
                        }
                    } else
                    {
                        stream_buf = std::make_unique<ZipCryptoIstreamBuf>(*ifs_.get(), decoder, real_offset + 12, meta.size);  // meta.size 使用的是 uncompression_size
                        break;
                    }
                }
                // 使用 last_mod_time 的高16位二次判断
                if (static_cast<uint8_t>((lfh.last_mod_time >> 8) & 0xFF) == zip_crypto_header[11])
                {
                    decoder = encryption::ZipCrypto{password_};
                    stream_buf = std::make_unique<ZipCryptoIstreamBuf>(*ifs_.get(), decoder, real_offset + 12, meta.size);  // meta.size 使用的是 uncompression_size
                } else
                {
                    invalid_password_ = true;
                    break;
                }
            }
            else if (lfh.general_purpose & static_cast<uint16_t>(ZipGeneralPurposeBitFlag::StrongEncryption))
            {
                // 设置了 StrongEncryption bit 后在文件前要插入一段 decryption header record 校验密码是否正确
                ifs_->seekg(static_cast<long long>(real_offset), std::ios::beg);
                if (const auto de_header = get_decryption_header(*ifs_.get()); !de_header.format)
                {
                    break;
                } else
                {
                    const size_t raw_file_offset = static_cast<size_t>(ifs_->tellg());
                    if (encryption_method == ZipEncryptionMethod::Unknown)
                    {
                        encryption_method = de_header.alg_id;
                    }
                    if (encryption_method == ZipEncryptionMethod::Unknown || encryption_method == ZipEncryptionMethod::ZipCrypto)
                    {
                        break;
                    }
                    if (encryption_method == ZipEncryptionMethod::RC4)
                    {
                        std::vector rc4_pwd(password_);
                        rc4_pwd.resize(de_header.bit_len, 0);
                        std::vector de_data(de_header.v_data);
                        encryption::RC4 rc4_decoder(rc4_pwd);
                        rc4_decoder.process(de_data.data(), de_data.size());
                        // 校验密码是否正确
                        if (crc::crc32(reinterpret_cast<const char*>(de_data.data()), de_data.size())
                            != de_header.v_crc32)
                        {
                            invalid_password_ = true;
                            break;
                        }
                        rc4_decoder = encryption::RC4(rc4_pwd);
                        stream_buf = std::make_unique<RC4IstreamBuf>(*ifs_.get(), rc4_decoder, raw_file_offset, meta.size);
                    } else break;
                }
            }
            else
            {
                break;
            }
        } while (false);
        if (!stream_buf)
        {
            stream_buf = std::make_unique<ZipIstreamBuf>(*ifs_.get(), 0, 0);
        }
    } else
    {
        stream_buf = std::make_unique<ZipIstreamBuf>(*ifs_.get(), real_offset, meta.size);
    }

    zip_stream = std::make_unique<ZipIstream>(std::move(stream_buf), meta);
    return zip_stream;
}

// 完成zip归档创建
void ZipFile::close() {
    if (!is_valid_) {
        return;
    }
    if (ifs_)
    {
        if (ifs_->is_open()) ifs_->close();
        is_valid_ = false;
        return;
    }

    // 计算中央目录的偏移量和大小
    uint32_t central_directory_offset = ofs_->tellp();
    uint32_t central_directory_size = 0;
    uint16_t total_central_directory_records = 0;

    // 写入中央目录
    auto stmt = db_.create_statement(
        "SELECT " + db::ZipInitializationStrategy::SQLEntityColumns +
        " FROM zip_entity ORDER BY filename ASC;"
    );
    auto rs = db_.query<db::ZipInitializationStrategy::SQLZipEntity>(std::move(stmt));
    for (const auto& entity : rs)
    {
        total_central_directory_records++;
        auto [file_name, extra_field, file_comment, record] = sql_entity_to_cdfh(entity);
        cdfh_host_to_le(record);
        ofs_->write(reinterpret_cast<const char*>(&record), sizeof(ZipCentralDirectoryFileHeader));
        central_directory_size += sizeof(ZipCentralDirectoryFileHeader);
        // ReSharper disable once CppRedundantCastExpression
        ofs_->write(reinterpret_cast<const char*>(file_name.c_str()), static_cast<long long>(file_name.size()));
        central_directory_size += static_cast<uint32_t>(file_name.size());
        if (!extra_field.empty())
        {
            ofs_->write(reinterpret_cast<const char*>(extra_field.data()), static_cast<long long>(extra_field.size()));
            central_directory_size += static_cast<uint32_t>(extra_field.size());
        }
        if (!file_comment.empty())
        {
            // ReSharper disable once CppRedundantCastExpression
            ofs_->write(reinterpret_cast<const char*>(file_comment.c_str()), static_cast<long long>(file_comment.size()));
            central_directory_size += static_cast<uint32_t>(file_comment.size());
        }
    }

    ZipEndOfCentralDirectoryRecord eocd {
        ZipFileHeaderSignature::EndOfCentralDirectory,
        0,
        0,
        static_cast<uint16_t>(total_central_directory_records),
        static_cast<uint16_t>(total_central_directory_records),
        central_directory_size,
        central_directory_offset,
        static_cast<uint16_t>(comment_.size())
    };
    eocd_host_to_le(eocd);
    ofs_->write(reinterpret_cast<const char*>(&eocd), sizeof(eocd));
    if (!comment_.empty())
    {
        // ReSharper disable once CppRedundantCastExpression
        ofs_->write(reinterpret_cast<const char*>(comment_.data()), static_cast<long long>(comment_.size()));
    }
    // 关闭文件
    ofs_->close();
    is_valid_ = false;
}
FileEntityMeta ZipFile::cdfh_to_file_meta(const CentralDirectoryEntry& cdfh)
{
    FileEntityMeta meta {
        std::filesystem::path(cdfh.file_name),
        FileEntityType::RegularFile,
        cdfh.record.uncompressed_size,
        std::chrono::system_clock::from_time_t(dos_to_unix_time(cdfh.record.last_mod_date, cdfh.record.last_mod_time)),
        std::chrono::system_clock::from_time_t(dos_to_unix_time(cdfh.record.last_mod_date, cdfh.record.last_mod_time)),
        std::chrono::system_clock::from_time_t(dos_to_unix_time(cdfh.record.last_mod_date, cdfh.record.last_mod_time)),
        0,
        0,
        0,
        {},
        {},
        0,
        {},
        0,
        0
    };
    std::vector<std::vector<uint8_t>> extra_fields;
    for (long long i = 0; i + 4 < cdfh.extra_field.size();)
    {
        const uint16_t length = le16toh(*reinterpret_cast<const uint16_t*>(&cdfh.extra_field[i+2]));
        if (i + 4 + length > cdfh.extra_field.size()) break;
        std::vector<uint8_t> ef(4+length);
        if (i + 4 + length == cdfh.extra_field.size())
        {
            ef.insert(ef.end(), cdfh.extra_field.begin() + i, cdfh.extra_field.end());
        } else
        {
            ef.insert(ef.end(), cdfh.extra_field.begin() + i, cdfh.extra_field.begin() + i + 4 + length);
        }
        extra_fields.push_back(ef);
        i += 4 + length;
    }
    if (cdfh.record.version_made_by == ZipVersionMadeBy::MS_DOS || cdfh.record.version_made_by == ZipVersionMadeBy::NTFS)
    {
        meta.windows_attributes = cdfh.record.external_file_attributes;
        for (const auto& field: extra_fields)
        {
            if (meta.windows_attributes & 0x10)
            {
                meta.type = FileEntityType::Directory;
            }
            if (const auto [ntfs_field, ok] = extra_field2ntfs(field); ok)
            {
                meta.creation_time = ntfs_u64_to_time_point(ntfs_field.c_time);
                meta.access_time = ntfs_u64_to_time_point(ntfs_field.a_time);
                meta.modification_time = ntfs_u64_to_time_point(ntfs_field.m_time);
                break;
            }
        }
    } else if (cdfh.record.version_made_by == ZipVersionMadeBy::Unix)
    {
        meta.posix_mode = (cdfh.record.external_file_attributes >> 16) & 0xFFFF;
        switch (meta.posix_mode & 0xF000) {
            case 0xA000: meta.type = FileEntityType::SymbolicLink; break;
            case 0x4000: meta.type = FileEntityType::Directory;    break;
            case 0x8000: meta.type = FileEntityType::RegularFile;   break;
            case 0x6000: meta.type = FileEntityType::BlockDevice;   break;
            case 0x2000: meta.type = FileEntityType::CharacterDevice; break;
            default:     meta.type = FileEntityType::RegularFile;   break;
        }

        for (const auto& field: extra_fields)
        {
            if (const auto [unix_field, ok] = extra_field2unix(field); ok)
            {
                meta.access_time = unix_u32_to_time_point(unix_field.a_time);
                meta.modification_time = unix_u32_to_time_point(unix_field.m_time);
                meta.uid = unix_field.uid;
                meta.gid = unix_field.gid;
                break;
            }
        }
    }
    update_file_entity_meta(meta);
    return meta;
}
std::vector<std::vector<uint8_t>> ZipFile::get_extra_field_list(const std::vector<uint8_t>& extra_field)
{
    std::vector<std::vector<uint8_t>> extra_fields;
    for (long long i = 0; i + 4 < extra_field.size();)
    {
        const uint16_t length = le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[i+2]));
        if (i + 4 + length > extra_field.size()) break;
        std::vector<uint8_t> ef(4+length);
        if (i + 4 + length == extra_field.size())
        {
            ef.insert(ef.end(), extra_field.begin() + i, extra_field.end());
        } else
        {
            ef.insert(ef.end(), extra_field.begin() + i, extra_field.begin() + i + 4 + length);
        }
        extra_fields.push_back(ef);
        i += 4 + length;
    }
    return {extra_fields};
}
//   扩展字段结构根据ZIP标准生成
//   see https://pkwaredownloads.blob.core.windows.net/pkware-general/Documentation/APPNOTE-6.3.9.TXT
//   NTFS 字段结构
// | NTFS Sig (2 bytes) | Ext Len (2 bytes)  | Reserved (4 bytes) | Tag1 (2 bytes) | Size1 (2 bytes) | MTime (8 bytes) | ATime (8 bytes) | CTime (8 bytes) |
// |--------------------|--------------------|--------------------|----------------|-----------------|-----------------|-----------------|-----------------|
// | 0x000A             | Length Following   | 0x0000             | 0x0001         | 24              | ...             | ...             | ...             |
std::pair<ZipFile::NTFSExtraField, bool> ZipFile::extra_field2ntfs(const std::vector<uint8_t>& extra_field)
{
    if (extra_field.size() < 4)
    {
        return {{}, false};
    }
    // Intel low-byte order
    const uint16_t signature = le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[0])),
    length = le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[2]));
    if (signature != 0x000A || length < 32 || extra_field.size() < length + 4)
    {
        return {{}, false};
    }
    const uint16_t tag1 = le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[8])),
    size1 = le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[10]));
    if (tag1 != 0x0001 || size1 != 24)
    {
        return {{}, false};
    }
    return {
        {
            le64toh(*reinterpret_cast<const uint64_t*>(&extra_field[12])),
            le64toh(*reinterpret_cast<const uint64_t*>(&extra_field[20])),
            le64toh(*reinterpret_cast<const uint64_t*>(&extra_field[28])),
        },
        true
    };
}
std::vector<uint8_t> ZipFile::ntfs2extra_field(const uint64_t m_time, const uint64_t a_time, const uint64_t c_time)
{
    std::vector<uint8_t> extra_field(36);
    *reinterpret_cast<uint16_t*>(&extra_field[0])  = htole16(0x000A);     // NTFS Signature
    *reinterpret_cast<uint16_t*>(&extra_field[2])  = htole16(32);         // Length Following
    *reinterpret_cast<uint32_t*>(&extra_field[4])  = htole32(0);          // Reserved
    *reinterpret_cast<uint16_t*>(&extra_field[8])  = htole16(0x0001);     // Tag1
    *reinterpret_cast<uint16_t*>(&extra_field[10]) = htole16(24);        // Size1
    *reinterpret_cast<uint64_t*>(&extra_field[12]) = htole64(m_time);    // MTime
    *reinterpret_cast<uint64_t*>(&extra_field[20]) = htole64(a_time);    // ATime
    *reinterpret_cast<uint64_t*>(&extra_field[28]) = htole64(c_time);    // CTime
    return extra_field;
}
//   Unix 字段结构
// | Unix Sig (2 bytes) | Ext Len (2 bytes)  | ATime (4 bytes) | MTime (4 bytes) | UID (2 bytes) | GID (2 bytes) |
// |--------------------|--------------------|-----------------|-----------------|---------------|---------------|
// | 0x000D             | Length Following   | ...             | ...             | ...           |               |
std::pair<ZipFile::UnixExtraField, bool> ZipFile::extra_field2unix(const std::vector<uint8_t>& extra_field)
{
    if (extra_field.size() < 4)
    {
        return {{}, false};
    }
    // Intel low-byte order
    const uint16_t signature = le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[0])),
    length = le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[2]));
    if (signature != 0x000d || length < 20 || extra_field.size() < length + 4)
    {
        return {{}, false};
    }
    const uint32_t a_time = le32toh(*reinterpret_cast<const uint32_t*>(&extra_field[4])),
    m_time = le32toh(*reinterpret_cast<const uint32_t*>(&extra_field[8]));
    const uint16_t uid = le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[12])),
    gid = le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[14]));
    return {
        {
            a_time,
            m_time,
            uid,
            gid
        },
        true
    };
}
std::vector<uint8_t> ZipFile::unix2extra_field(const uint32_t a_time, const uint32_t m_time, const uint16_t uid, const uint16_t gid)
{
    std::vector<uint8_t> extra_field(16);
    *reinterpret_cast<uint16_t*>(&extra_field[0])  = htole16(0x000D);   // Unix Signature
    *reinterpret_cast<uint16_t*>(&extra_field[2])  = htole16(12);       // Length Following
    *reinterpret_cast<uint32_t*>(&extra_field[4])  = htole32(a_time);   // ATime
    *reinterpret_cast<uint32_t*>(&extra_field[8])  = htole32(m_time);   // MTime
    *reinterpret_cast<uint16_t*>(&extra_field[12]) = htole16(uid);      // UID
    *reinterpret_cast<uint16_t*>(&extra_field[14]) = htole16(gid);      // GID
    return extra_field;
}
// Strong Encryption Header (0x0017), in local file header
// | Sig (2 bytes) | Ext Len (2 bytes) | Format (2 bytes) | AlgID (2 bytes)     | BitLen (2 bytes) | Flags (2 bytes)  | Cert Data (variable) |
// |---------------|-------------------|------------------|---------------------|------------------|------------------|----------------------|
// | 0x0017        | Length Following  | ...              | ZipEncryptionMethod | 128, 256, etc.   | ...              | ...                  |
ZipFile::StrongEncryptionHeader ZipFile::get_encryption_method(const std::vector<uint8_t>& extra_field)
{
    if (extra_field.size() < 4) return {0, ZipEncryptionMethod::Unknown};
    const uint16_t signature = le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[0])),
    length = le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[2]));
    if (signature != 0x0017 || length + 4 < extra_field.size()) return {0, ZipEncryptionMethod::Unknown};
    std::vector<uint8_t> cert_data;
    if (length > 8)
    {
        cert_data.resize(length - 8);
        std::copy(extra_field.begin() + 12, extra_field.begin() + 12 + length - 8, cert_data.begin());
    }
    return {
        le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[4])),
        static_cast<ZipEncryptionMethod>(le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[6]))),
        le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[8])),
        le16toh(*reinterpret_cast<const uint16_t*>(&extra_field[10])),
        {cert_data}
    };
}
//  Strong Encryption Header(0x0017 in central directory extra field)
//  WARNING! it is different from AES extra field(0x9901)!
//  more information for AE-x extra field, see https://www.winzip.com/en/support/aes-encryption/
// | IVSize (2 bytes) | IVData (IVSize bytes) | Size (4 bytes) | Format (2 bytes) | AlgID (2 bytes) | BitLen (2 bytes) | Flags (2 bytes) |
// |------------------|-----------------------|----------------|------------------|-----------------|------------------|-----------------|
// | ...              | ...                   | Length Follow  | 3                | ...             | 32 - 448         | ...             |
// | ErdSize (2 bytes)| ErdData (ErdSize bytes) | ResSize (4 bytes) | Reserved (ResSize bytes) | VSize (2 bytes)        | VData (VSize-4 bytes) | VCRC32 (4 bytes) |
// |------------------|-------------------------|-------------------|--------------------------|------------------------|-----------------------|------------------|
// | ...              | ...                     | ...               | ...                      | include 4 bytes VCRC32 | ...                   | ...              |
ZipFile::DecryptionHeaderRecord
ZipFile::get_decryption_header(std::istream& is)
{
#define SAFE_READ16(param) \
    is.read(reinterpret_cast<char*>(&(param)), sizeof(param)); \
    param = le16toh(param); \
    if (is.gcount() != sizeof(param)) return {};
#define SAFE_READ32(param) \
    is.read(reinterpret_cast<char*>(&(param)), sizeof(param)); \
    param = le32toh(param); \
    if (is.gcount() != sizeof(param)) return {};

    uint16_t u16_cache;
    uint32_t u32_cache, length;
    DecryptionHeaderRecord decryption_header;
    SAFE_READ16(u16_cache);
    decryption_header.iv_data.resize(u16_cache);
    is.read(reinterpret_cast<char*>(decryption_header.iv_data.data()), u16_cache);
    if (is.gcount() != u16_cache) return {};
    is.read(reinterpret_cast<char*>(&length), sizeof(length));
    if (is.gcount() != sizeof(length) || length < 20) return {};
    SAFE_READ16(decryption_header.format);
    length -= 2;
    SAFE_READ16(decryption_header.alg_id);
    length -= 2;
    SAFE_READ16(decryption_header.bit_len);
    length -= 2;
    SAFE_READ16(decryption_header.flags);
    length -= 2;

    SAFE_READ16(u16_cache);
    length -= 2;
    if (length < u16_cache + 4 + 2 + 4) return {};
    decryption_header.erd_data.resize(u16_cache);
    is.read(reinterpret_cast<char*>(decryption_header.erd_data.data()), u16_cache);
    if (is.gcount() != u16_cache) return {};
    length -= u16_cache;
    SAFE_READ32(u32_cache);
    length -= 4;
    if (length < u32_cache + 2 + 4) return {};
    decryption_header.reserved.resize(u32_cache);
    is.read(reinterpret_cast<char*>(decryption_header.reserved.data()), u32_cache);
    if (is.gcount() != u32_cache) return {};
    length -= u32_cache;
    SAFE_READ16(u16_cache);
    length -= 2;
    if (length < u16_cache) return {};
    decryption_header.v_data.resize(u16_cache-4);
    is.read(reinterpret_cast<char*>(decryption_header.v_data.data()), u16_cache - 4);
    if (is.gcount() != u16_cache - 4) return {};
    length -= u16_cache - 4;
    SAFE_READ32(decryption_header.v_crc32);

#undef SAFE_READ16
#undef SAFE_READ32
    return decryption_header;
}

std::vector<uint8_t> ZipFile::make_decryption_header(const DecryptionHeaderRecord& dhr)
{
    const size_t total_size = 2 + dhr.iv_data.size() + 4 + 2 + 2 + 2 + 2
                              + 2 + dhr.erd_data.size() + 4 + dhr.reserved.size()
                              + 2 + dhr.v_data.size() + 4;
    std::vector<uint8_t> header(total_size);
    size_t offset = 0;
    *reinterpret_cast<uint16_t*>(&header[offset]) = htole16(static_cast<uint16_t>(dhr.iv_data.size()));
    offset += 2;
    std::copy(dhr.iv_data.begin(), dhr.iv_data.end(), header.begin() + offset);
    offset += dhr.iv_data.size();
    *reinterpret_cast<uint32_t*>(&header[offset]) = htole32(static_cast<uint32_t>(total_size - offset - 4));
    offset += 4;
    *reinterpret_cast<uint16_t*>(&header[offset]) = htole16(dhr.format);
    offset += 2;
    *reinterpret_cast<uint16_t*>(&header[offset]) = htole16(static_cast<uint16_t>(dhr.alg_id));
    offset += 2;
    *reinterpret_cast<uint16_t*>(&header[offset]) = htole16(dhr.bit_len);
    offset += 2;
    *reinterpret_cast<uint16_t*>(&header[offset]) = htole16(dhr.flags);
    offset += 2;
    *reinterpret_cast<uint16_t*>(&header[offset]) = htole16(static_cast<uint16_t>(dhr.erd_data.size()));
    offset += 2;
    std::copy(dhr.erd_data.begin(), dhr.erd_data.end(), header.begin() + offset);
    offset += dhr.erd_data.size();
    *reinterpret_cast<uint32_t*>(&header[offset]) = htole32(static_cast<uint32_t>(dhr.reserved.size()));
    offset += 4;
    std::copy(dhr.reserved.begin(), dhr.reserved.end(), header.begin() + offset);
    offset += dhr.reserved.size();
    *reinterpret_cast<uint16_t*>(&header[offset]) = htole16(static_cast<uint16_t>(dhr.v_data.size() + 4));
    offset += 2;
    std::copy(dhr.v_data.begin(), dhr.v_data.end(), header.begin() + offset);
    offset += dhr.v_data.size();
    *reinterpret_cast<uint32_t*>(&header[offset]) = htole32(dhr.v_crc32);
    return header;
}
