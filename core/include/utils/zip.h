//
// Created by ycm on 2025/10/1.
//

#ifndef BACKUPSUITE_ZIP_H
#define BACKUPSUITE_ZIP_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "api.h"
#include "utils/database.h"
#include "filesystem/entities.h"

#ifdef _MSC_VER
#pragma warning(disable:4200)  // 禁用 MSVC 零数组警告
#define ZERO_ARRAY(TYPE, NAME) TYPE NAME[0]
#elif defined(__GNUC__) || defined(__clang__)
#define ZERO_ARRAY(TYPE, NAME) TYPE NAME[]
#else
#define ZERO_ARRAY(TYPE, NAME) TYPE NAME[1]
#endif

namespace zip
{
    class BACKUP_SUITE_API ZipInitializationStrategy final : public db::DatabaseInitializationStrategy
    {
      [[nodiscard]] std::string get_initialization_sql() const override
      {
          return (
              "CREATE TABLE IF NOT EXISTS zip_entity("
              "id INTEGER PRIMARY KEY AUTOINCREMENT,"
              ");"
          );
      }
    };

    namespace header
    {
enum class ZipFileHeaderSignature : uint32_t
{
    Unknown = 0,
    LocalFile = 0x04034b50,
    CentralDirectoryFile = 0x02014b50,
    EndOfCentralDirectory = 0x06054b50,
    Zip64EndOfCentralDirectoryLocator = 0x07064b50,
    Zip64EndOfCentralDirectoryRecord = 0x06064b50,
};

enum class ZipVersionMadeBy : uint16_t
{
    MS_DOS = 0,
    Amiga = 1,
    OpenVMS = 2,
    Unix = 3,
    VM_CMS = 4,
    Atari_ST = 5,
    OS2_HPFS = 6,
    Macintosh = 7,
    Z_System = 8,
    CP_M = 9,
    NTFS = 10,
    MVS = 11,
    VSE = 12,
    Acorn_RISC = 13,
    VFAT = 14,
    Alternate_MVS = 15,
    BeOS = 16,
    Tandem = 17,
    OS400 = 18,
    OSX = 19,
    Thru255 = 20,   // unused
    Unknown = 255
};

enum class ZipVersionNeeded: uint16_t
{
    Unknown = 0,
    Version10 = 10,   // 1.0
    Version11 = 11,   // 1.1
    Version20 = 20,   // 2.0
    Version21 = 21,   // 2.1
    Version25 = 25,   // 2.5
    Version27 = 27,   // 2.7
    Version45 = 45,   // 4.5
    Version46 = 46,   // 4.6
    Version50 = 50,   // 5.0
    Version51 = 51,   // 5.1
    Version52 = 52,   // 5.2
    Version61 = 61,   // 6.1
    Version62 = 62,   // 6.2
    Version63 = 63,   // 6.3
};

enum class ZipGeneralPurposeBitFlag : uint16_t
{
    Encrypted = 1 << 0,
    CompressionOption1 = 1 << 1,
    CompressionOption2 = 1 << 2,
    DataDescriptor = 1 << 3,
    EnhancedDeflation = 1 << 4, // reserved
    CompressedPatchedData = 1 << 5,
    StrongEncryption = 1 << 6,
    // bits 7-10 are unused
    Utf8Encoding = 1 << 11,
    // bit 12 is reserved by PKWARE for enhanced compression
    MaskHeaderValues = 1 << 13,
    // bit 14 is reserved by PKWARE for alternative streams
    // bit 15 is reserved by PKWARE
};

enum class ZipCompressionMethod : uint16_t
{
    Store = 0,
    Shrink = 1,
    Reduce1 = 2,
    Reduce2 = 3,
    Reduce3 = 4,
    Reduce4 = 5,
    Implode = 6,
    Tokenizing = 7, // reserved
    Deflate = 8,
    Deflate64 = 9,
    PKWARE_DCLImplode = 10,
    // 11 is reserved by PKWARE
    BZip2 = 12,
    // 13 is reserved by PKWARE
    LZMA = 14,
    // 15 is reserved by PKWARE
    IBM_Cmpsc = 16,
    // 17 is reserved by PKWARE
    IBM_Terse = 18,
    IBM_LZ77z = 19,
    // 20 is deprecated, use 93 instead
    ZSTD = 93,
    MP3 = 94,
    XZ = 95,
    JPEG = 96,
    WavPack = 97,
    PPMd_V1 = 98,
    AES_Encryption = 99,
    Unknown = 65535
};

struct BACKUP_SUITE_API ZipLocalFileHeader
{
    ZipFileHeaderSignature signature = ZipFileHeaderSignature::LocalFile;
    // 0x04034b50
    ZipVersionNeeded version_needed;       // 版本需要
    uint16_t general_purpose;      // 通用位标志
    ZipCompressionMethod compression_method;   // 压缩方法
    uint16_t last_mod_time;        // 最后修改时间
    uint16_t last_mod_date;        // 最后修改日期
    uint32_t crc32;                // CRC-32 校验码
    uint32_t compressed_size;      // 压缩后的大小
    uint32_t uncompressed_size;    // 未压缩的大小
    uint16_t file_name_length;     // 文件名长度
    uint16_t extra_field_length;   // 扩展字段长度
    ZERO_ARRAY(uint8_t, file_name);// 文件名，长度为 file_name_length
    // 后面跟着 file_name 和 extra_field
};

struct BACKUP_SUITE_API ZipCentralDirectoryFileHeader
{
    ZipFileHeaderSignature signature = ZipFileHeaderSignature::CentralDirectoryFile;
    // 0x02014b50
    ZipVersionMadeBy version_made_by;         // 创建版本
    ZipVersionNeeded version_needed;          // 版本需要
    uint16_t general_purpose;         // 通用位标志
    ZipCompressionMethod compression_method;      // 压缩方法
    uint16_t last_mod_time;           // 最后修改时间
    uint16_t last_mod_date;           // 最后修改日期
    uint32_t crc32;                   // CRC-32 校验码
    uint32_t compressed_size;         // 压缩后的大小
    uint32_t uncompressed_size;       // 未压缩的大小
    uint16_t file_name_length;        // 文件名长度
    uint16_t extra_field_length;      // 扩展字段长度
    uint16_t file_comment_length;     // 文件注释长度
    uint16_t disk_number_start;       // 文件起始磁盘号
    uint16_t internal_file_attributes;// 内部文件属性
    uint32_t external_file_attributes;// 外部文件属性
    uint32_t local_header_offset;     // 本地文件头偏移量
    ZERO_ARRAY(uint8_t, file_name);   // 文件名，长度为 file_name_length
    // 后面跟着 file_name、extra_field 和 file_comment
};

struct BACKUP_SUITE_API ZipEndOfCentralDirectoryRecord
{
    ZipFileHeaderSignature signature = ZipFileHeaderSignature::EndOfCentralDirectory;
    // 0x06054b50
    uint16_t disk_number;                 // 当前磁盘号
    uint16_t central_directory_start_disk;// 中央目录起始磁盘号
    uint16_t num_central_directory_records_on_this_disk; // 当前磁盘上的中央目录记录数
    uint16_t total_central_directory_records; // 中央目录记录总数
    uint32_t central_directory_size;      // 中央目录大小
    uint32_t central_directory_offset;    // 中央目录偏移量
    uint16_t comment_length;             // 注释长度
    ZERO_ARRAY(uint8_t, comment);        // 注释，长度为 comment_length
};

struct BACKUP_SUITE_API Zip64EndOfCentralDirectory
{
    ZipFileHeaderSignature signature = ZipFileHeaderSignature::Zip64EndOfCentralDirectoryRecord;
    // 0x06064b50
    uint64_t size_of_zip64_end_of_central_directory_record; // zip64
    ZipVersionMadeBy version_made_by;
    ZipVersionNeeded version_needed;
    uint32_t disk_number;
    uint32_t central_directory_start_disk;
    uint64_t num_central_directory_records_on_this_disk;
    uint64_t total_central_directory_records;
    uint64_t central_directory_size;
    uint64_t central_directory_offset;
    // 后面跟着可选的扩展数据
};

struct BACKUP_SUITE_API ZipDataDescriptor
{
    uint32_t crc32;                // CRC-32 校验码
    uint32_t compressed_size;      // 压缩后的大小
    uint32_t uncompressed_size;    // 未压缩的大小
};

struct BACKUP_SUITE_API ZipExtraField
{
    uint16_t header_id;        // 头标识
    uint16_t data_size;        // 数据大小
    ZERO_ARRAY(uint8_t, data); // 数据，长度为 data_size
};
    }

class BACKUP_SUITE_API ZipFile{
public:
    explicit ZipFile(std::string  zip_path, bool overwrite = true);
    ~ZipFile();

    // 禁止复制和移动
    ZipFile(const ZipFile&) = delete;
    ZipFile& operator=(const ZipFile&) = delete;
    ZipFile(ZipFile&&) = delete;
    ZipFile& operator=(ZipFile&&) = delete;

    /**
     * @brief 添加文件实体到zip归档
     * @param file ReadableFile对象，提供文件内容和元数据
     * @return 是否成功添加
     */
    bool add_entity(ReadableFile& file);

    /**
     * @brief 完成zip归档创建
     */
    void close();

    // 中央目录记录结构体，包含文件名
    // 这里必须保证不要直接将CentralDirectoryEntry写入内存，ZipCentralDirectoryFileHeader的file_name由开头的std::string管理
    struct CentralDirectoryEntry {
        std::string file_name;
        header::ZipCentralDirectoryFileHeader record;
    };

    // 预留接口：将FileEntityMeta转换为Zip本地文件头
    static header::ZipLocalFileHeader convert_to_local_file_header(const FileEntityMeta& meta);

    // 预留接口：将FileEntityMeta转换为Zip中央目录记录
    static CentralDirectoryEntry convert_to_central_directory_record(
        const FileEntityMeta& meta,
        uint32_t local_header_offset,
        uint32_t compressed_size,
        uint32_t crc32);

private:

    std::ofstream m_zip_file;
    std::string m_zip_path;
    bool m_closed;

    // 中央目录记录
    std::vector<CentralDirectoryEntry> m_central_directory;

    // 辅助方法
    void write_local_file_header(const FileEntityMeta& meta);
    void write_central_directory_record(const FileEntityMeta& meta, uint32_t local_header_offset, uint32_t compressed_size, uint32_t crc32);
    void write_end_of_central_directory(uint32_t central_directory_offset, uint32_t central_directory_size);
};


} // namespace zip

#endif // BACKUPSUITE_ZIP_H
