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
#include "utils/database_strategies.h"
#include "filesystem/entities.h"
#include "utils/fs_deleter.h"

#ifdef _MSC_VER
#pragma warning(disable : 4200) // 禁用 MSVC 零数组警告
#define ZERO_ARRAY(TYPE, NAME) TYPE NAME[0]
#elif defined(__GNUC__) || defined(__clang__)
#define ZERO_ARRAY(TYPE, NAME) TYPE NAME[]
#else
#define ZERO_ARRAY(TYPE, NAME) TYPE NAME[1]
#endif

using fs_deleter::FStreamDeleter;
using fs_deleter::IFStreamPointer;
using fs_deleter::OFStreamPointer;

namespace zip
{
    // ZipInitializationStrategy is defined in database_strategies.h

    namespace header
    {
// 为了内存对齐，我们使用 pragma pack
#pragma pack(push, 1)
        enum class ZipFileHeaderSignature : uint32_t
        {
            Unknown = 0,
            LocalFile = 0x04034b50,
            CentralDirectoryFile = 0x02014b50,
            EndOfCentralDirectory = 0x06054b50,
            Zip64EndOfCentralDirectoryLocator = 0x07064b50,
            Zip64EndOfCentralDirectoryRecord = 0x06064b50,
            FileDescriptor = 0x02014b50,
        };
        enum class ZipVersionMadeBy : uint8_t
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
            Thru255 = 20, // unused
            Unknown = 255
        };
        enum class ZipVersionNeeded : uint16_t
        {
            Unknown = 0,
            Version10 = 10, // 1.0
            Version11 = 11, // 1.1
            Version20 = 20, // 2.0
            Version21 = 21, // 2.1
            Version25 = 25, // 2.5
            Version27 = 27, // 2.7
            Version45 = 45, // 4.5
            Version46 = 46, // 4.6
            Version50 = 50, // 5.0
            Version51 = 51, // 5.1
            Version52 = 52, // 5.2
            Version61 = 61, // 6.1
            Version62 = 62, // 6.2
            Version63 = 63, // 6.3
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
            ZipVersionNeeded version_needed{}; // 版本需要
            uint16_t general_purpose{}; // 通用位标志
            ZipCompressionMethod compression_method{}; // 压缩方法
            // MS_DOS 格式的时间和日期
            // 时间: 15-11 bit 小时, 10-5 bit 分钟, 4-0 bit 秒/2
            uint16_t last_mod_time{}; // 最后修改时间
            // 日期: 15-9 bit 年(从1980开始), 8-5 bit 月, 4-0 bit 日
            uint16_t last_mod_date{}; // 最后修改日期
            uint32_t crc32{}; // CRC-32 校验码
            uint32_t compressed_size{}; // 压缩后的大小
            uint32_t uncompressed_size{}; // 未压缩的大小
            uint16_t file_name_length{}; // 文件名长度
            uint16_t extra_field_length{}; // 扩展字段长度
            // ZERO_ARRAY(uint8_t, file_name); // 文件名，长度为 file_name_length
            // 后面跟着 file_name 和 extra_field
        };
        struct BACKUP_SUITE_API ZipCentralDirectoryFileHeader
        {
            ZipFileHeaderSignature signature = ZipFileHeaderSignature::CentralDirectoryFile;
            // 0x02014b50
            uint8_t generator_version{};  // version_made_by中的低8位表示创建zip工具的版本号
            ZipVersionMadeBy version_made_by{}; // 创建版本
            ZipVersionNeeded version_needed{}; // 版本需要
            uint16_t general_purpose{}; // 通用位标志
            ZipCompressionMethod compression_method{}; // 压缩方法
            uint16_t last_mod_time{}; // 最后修改时间
            uint16_t last_mod_date{}; // 最后修改日期
            uint32_t crc32{}; // CRC-32 校验码
            uint32_t compressed_size{}; // 压缩后的大小
            uint32_t uncompressed_size{}; // 未压缩的大小
            uint16_t file_name_length{}; // 文件名长度
            uint16_t extra_field_length{}; // 扩展字段长度
            uint16_t file_comment_length{}; // 文件注释长度
            uint16_t disk_number_start{}; // 文件起始磁盘号
            // 1-2 bit PKWARE保留 0 bit 若为文本文件则为1
            // 1 bit 用于与 IBM 大型机遗留文件进行交互
            uint16_t internal_file_attributes{}; // 内部文件属性
            uint32_t external_file_attributes{}; // 外部文件属性
            uint32_t local_header_offset{}; // 本地文件头偏移量
            // ZERO_ARRAY(uint8_t, file_name); // 文件名，长度为 file_name_length
            // 后面跟着 file_name、extra_field 和 file_comment
        };
        struct BACKUP_SUITE_API ZipEndOfCentralDirectoryRecord
        {
            ZipFileHeaderSignature signature = ZipFileHeaderSignature::EndOfCentralDirectory;
            // 0x06054b50
            uint16_t disk_number{}; // 当前磁盘号
            uint16_t central_directory_start_disk{}; // 中央目录起始磁盘号
            uint16_t num_central_directory_records_on_this_disk{}; // 当前磁盘上的中央目录记录数
            uint16_t total_central_directory_records{}; // 中央目录记录总数
            uint32_t central_directory_size{}; // 中央目录大小
            uint32_t central_directory_offset{}; // 中央目录偏移量
            uint16_t comment_length{}; // 注释长度
            // ZERO_ARRAY(uint8_t, comment); // 注释，长度为 comment_length
        };
        struct BACKUP_SUITE_API Zip64EndOfCentralDirectory
        {
            ZipFileHeaderSignature signature = ZipFileHeaderSignature::Zip64EndOfCentralDirectoryRecord;
            // 0x06064b50
            uint64_t size_of_zip64_end_of_central_directory_record{}; // zip64
            ZipVersionMadeBy version_made_by{};
            ZipVersionNeeded version_needed{};
            uint32_t disk_number{};
            uint32_t central_directory_start_disk{};
            uint64_t num_central_directory_records_on_this_disk{};
            uint64_t total_central_directory_records{};
            uint64_t central_directory_size{};
            uint64_t central_directory_offset{};
            // 后面跟着可选的扩展数据
        };
        struct BACKUP_SUITE_API ZipDataDescriptor
        {
            uint32_t crc32; // CRC-32 校验码
            uint32_t compressed_size; // 压缩后的大小
            uint32_t uncompressed_size; // 未压缩的大小
        };
        struct BACKUP_SUITE_API Zip64DataDescriptor
        {
            uint32_t crc32; // CRC-32 校验码
            uint64_t compressed_size; // 压缩后的大小
            uint64_t uncompressed_size; // 未压缩的大小
        };
        struct BACKUP_SUITE_API ZipExtraField
        {
            uint16_t header_id; // 头标识
            uint16_t data_size; // 数据大小
            // ZERO_ARRAY(uint8_t, data); // 数据，长度为 data_size
        };
#pragma pack(pop)
    } // namespace header

#ifdef _WIN32
    // constexpr auto SYSTEM_VERSION_MADE_BY = header::ZipVersionMadeBy::MS_DOS;
    constexpr auto SYSTEM_VERSION_MADE_BY = header::ZipVersionMadeBy::NTFS; // 部分软件对NTFS支持不行,但我们仍然使用NTFS标记
#elif defined(__APPLE__)
    constexpr auto SYSTEM_VERSION_MADE_BY = header::ZipVersionMadeBy::Macintosh;
#elif defined(__linux__)
    constexpr auto SYSTEM_VERSION_MADE_BY = header::ZipVersionMadeBy::Unix;
#else
    static_assert(0, "unknown platform");
#endif

    class BACKUP_SUITE_API ZipFile
    {
      private:
        class ZipIstreamBuf: public std::streambuf
        {
            std::ifstream &file_;
            int offset_;
            size_t size_;
            static constexpr size_t buffer_size_ = 8192;
            char buffer_[buffer_size_] = {};
        public:
            explicit ZipIstreamBuf(std::ifstream &ifs, int offset, size_t size):
                file_(ifs), offset_(offset), size_(size)
            {
                setg(nullptr, nullptr, nullptr);
                if (offset < 0 || size == 0 || !ifs.is_open() || ifs.eof())
                {
                    offset_ = size_ = 0;
                    return;
                }
            }
            ~ZipIstreamBuf() override = default;
            int_type underflow() override
            {
                if (gptr() < egptr())
                {
                    return traits_type::to_int_type(*gptr());
                }
                file_.seekg(offset_, std::ios::beg);
                size_t to_read = std::min(buffer_size_, size_);
                file_.read(buffer_, to_read);
                offset_ += to_read;
                size_ -= to_read;
                if (to_read == 0 || file_.eof() || file_.gcount() != static_cast<std::streamsize>(to_read))
                {
                    return traits_type::eof();
                }
                setg(buffer_, buffer_, buffer_ + to_read);
                return traits_type::to_int_type(*gptr());
            }
        };

        class ZipIstream: public std::istream
        {
            FileEntityMeta meta_;
            std::ifstream &file_;
            std::unique_ptr<ZipIstreamBuf> buffer_;
        public:
            ZipIstream(std::ifstream &ifs, const int offset, const FileEntityMeta &meta):
                std::istream(nullptr), meta_(meta), file_(ifs), buffer_(std::make_unique<ZipIstreamBuf>(ifs, offset, meta.size))
            {
                rdbuf(buffer_.get());
                if (!meta_.size || !ifs.is_open() || ifs.eof())
                {
                    setstate(std::ios::eofbit);
                }
            }
            ~ZipIstream() override = default;
            FileEntityMeta get_meta() { return meta_; }
        };

      public:
        // 中央目录记录结构体，包含文件名
        // 这里必须保证不要直接将CentralDirectoryEntry写入内存，ZipCentralDirectoryFileHeader的file_name由开头的std::string管理
        struct CentralDirectoryEntry
        {
            std::string file_name;
            std::vector<uint8_t> extra_field;
            std::string file_comment;
            header::ZipCentralDirectoryFileHeader record;
        };
        enum ZipMode
        {
            input,
            output
        };
        explicit ZipFile(const std::filesystem::path& path, const FStreamDeleter<std::ifstream>& ifsDeleter = FStreamDeleter<std::ifstream>(),
                         const FStreamDeleter<std::ofstream>& ofsDeleter = FStreamDeleter<std::ofstream>()) : db_(std::move(std::make_unique<db::ZipInitializationStrategy>()).get()), ifs_(nullptr, ifsDeleter), ofs_(nullptr, ofsDeleter)
        {
            if (std::filesystem::exists(path))
            {
                ZipFile(path, ZipMode::input, ifsDeleter, ofsDeleter);
            } else
            {
                ZipFile(path, ZipMode::output, ifsDeleter, ofsDeleter);
            }
        }
        ZipFile(const std::filesystem::path& path, ZipMode mode, const FStreamDeleter<std::ifstream>& ifsDeleter = FStreamDeleter<std::ifstream>(),
            const FStreamDeleter<std::ofstream>& ofsDeleter = FStreamDeleter<std::ofstream>()) : db_(std::move(std::make_unique<db::ZipInitializationStrategy>()).get()), ifs_(nullptr, ifsDeleter), ofs_(nullptr, ofsDeleter)
            {
                if (mode == ZipMode::input)
                {
                    ifs_ = IFStreamPointer(new std::ifstream(path, std::ios::binary), FStreamDeleter<std::ifstream>());
                    if (!ifs_ || !ifs_->is_open())
                    {
                        is_valid_ = false;
                        return;
                    }
                    init_db_from_zip();
                } else
                {
                    ofs_ = OFStreamPointer(new std::ofstream(path, std::ios::binary | std::ios::trunc), FStreamDeleter<std::ofstream>());
                    // Check if the file was successfully opened
                    if (!ofs_ || !ofs_->is_open())
                    {
                        is_valid_ = false;
                    }
                }
            }
        ~ZipFile();

        // 禁止复制和移动
        ZipFile(const ZipFile&) = delete;
        ZipFile& operator=(const ZipFile&) = delete;
        ZipFile(ZipFile&&) = delete;
        ZipFile& operator=(ZipFile&&) = delete;

        /**
         * @brief 添加文件实体到zip归档
         * @param file ReadableFile对象，提供文件内容和元数据
         * @param compression_method
         * @return 是否成功添加
         */
        bool add_entity(ReadableFile& file, header::ZipCompressionMethod compression_method = header::ZipCompressionMethod::Store);

        /**
         * @brief 列出指定路径下的所有文件和目录
         * @param path 路径
         * @return 文件元数据和偏移量的列表
         */
        [[nodiscard]] std::vector<CentralDirectoryEntry> list_dir(const std::filesystem::path& path) const;

        /**
         * @brief 获取指定路径的文件流
         * @param path 文件路径
         * @return 文件流对象，如果文件不存在或不是普通文件则返回nullptr
         */
        [[nodiscard]] std::unique_ptr<ZipIstream> get_file_stream(const std::filesystem::path& path) const;

        /**
         * @brief 完成zip归档创建
         */
        void close();

        void set_version_made_by(const header::ZipVersionNeeded version)
        {
            version_make_by_ = version;
        }
        static FileEntityMeta cdfh_to_file_meta(const CentralDirectoryEntry&);
        // 预留接口：将FileEntityMeta转换为Zip本地文件头
        static header::ZipLocalFileHeader convert_to_local_file_header(const FileEntityMeta& meta);
        // 预留接口：将FileEntityMeta转换为Zip中央目录记录
        static CentralDirectoryEntry convert_to_central_directory_record(const FileEntityMeta& meta, uint32_t local_header_offset, uint32_t compressed_size, uint32_t crc32);

      private:
        IFStreamPointer ifs_;
        OFStreamPointer ofs_;
        bool is_valid_ = true;

        // 数据库成员变量
        db::Database db_;

        // 中央目录记录
        std::vector<CentralDirectoryEntry> m_central_directory{};
        header::ZipEndOfCentralDirectoryRecord eocd_{header::ZipFileHeaderSignature::Unknown};

        // zip的注释
        std::string comment_{};
        // zip相关信息
        header::ZipVersionNeeded version_make_by_ = header::ZipVersionNeeded::Version20;

        void init_db_from_zip();
        [[nodiscard]] bool insert_entity(const header::ZipCentralDirectoryFileHeader*, const std::string& file_name,
                                         const std::vector<uint8_t>& extra_field,
                                         const std::string& file_comment) const;

        void write_central_directory_record(const FileEntityMeta& meta, uint32_t local_header_offset,
                                            uint32_t compressed_size, uint32_t crc32);
        void write_end_of_central_directory(uint32_t central_directory_offset, uint32_t central_directory_size);

        // 扩展字段解读
        struct NTFSExtraField
        {
            uint64_t m_time, a_time, c_time;
        };
        static std::pair<NTFSExtraField, bool> extra_field_to_ntfs(const std::vector<uint8_t>& extra_field);
        static std::vector<uint8_t> ntfs_to_extra_field(uint64_t m_time=0, uint64_t a_time=0, uint64_t c_time=0);
        struct UnixExtraField
        {
            uint32_t a_time, m_time;
            uint16_t uid, gid;
        };
        static std::pair<UnixExtraField, bool> extra_field_to_unix(const std::vector<uint8_t>& extra_field);
        static std::vector<uint8_t> unix_to_extra_field(uint32_t a_time=0, uint32_t m_time=0, uint16_t uid=0, uint16_t gid=0);
    };


} // namespace zip

#endif // BACKUPSUITE_ZIP_H
