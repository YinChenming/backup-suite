//
// Created by ycm on 25-9-5.
//
#include "utils/tar.h"
#include "utils/database_strategies.h"

#include <sstream>
#include <unordered_map>

#include "utils/crc.h"

using namespace tar;

static std::string key_value2pax_field(const std::string& key, const std::string& value)
{
    if (value.empty() || key.empty()) return {};
    std::stringstream ss;
    ss << " " << key << "=" << value << "\n";
    const std::string content = ss.str();
    int len_len = 1;
    const auto content_size = content.size();
    size_t base10 = 10;
    while (len_len + content_size >= base10)
    {
        len_len++;
        base10 *= 10;
    }
    return std::to_string(len_len+content_size) + content;
}
static std::string time_point2pax_string(const std::chrono::system_clock::time_point& tp, const unsigned int nano_width = 9)
{
    const auto duration = tp.time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    const auto nano_seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - seconds).count();
    std::stringstream ss;
    if (nano_width || !nano_seconds)
    {
        ss << seconds.count();
    } else
    {
        ss << seconds.count() << "." << std::setfill('0') << std::setw(nano_width) << nano_seconds;
    }
    return ss.str();
}
static std::chrono::system_clock::time_point pax_string2time_point(const std::string& str)
{
    const auto dot_pos = str.find('.'), dot_rpos = str.rfind('.');
    long long seconds = 0, nano_seconds = 0;
    if (dot_pos == std::string::npos && dot_rpos == std::string::npos)
    {
        seconds = std::stoll(str);
    } else if (dot_pos == dot_rpos)
    {
        seconds = std::stoll(str.substr(0, dot_pos));
        // 精度对齐到小数点后9位
        auto nano_str = str.substr(dot_pos+1);
        if (nano_str.length() < 9)
        {
            nano_str.append(9 - nano_str.length(), '0');
        } else if (nano_str.length() > 9)
        {
            nano_str = nano_str.substr(0, 9);
        }
        nano_seconds = std::stoll(nano_str);
    }
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::seconds(seconds) + std::chrono::nanoseconds(nano_seconds)
        )
    );
}

void TarFile::init_db_from_tar()
{
    if (!is_valid_ || !ifs_ || !ifs_->is_open())
    {
        return;
    }
    char block[512];
    std::string long_name;
    std::map<std::string, std::string> pax_headers;
    bool previous_special_block = false;
    bool last_block_all_zero = false;
    ifs_->seekg(0, std::ios::end);
    size_t end_of_archive_offset = ifs_->tellg(), current_offset = 0;
    ifs_->seekg(0, std::ios::beg);
    while (current_offset < end_of_archive_offset)
    {
        ifs_->read(block, 512);
        current_offset += 512;
        if (ifs_->gcount() != 512)
        {
            is_valid_ = false;
            return;
        }
        if (std::all_of(std::begin(block), std::end(block), [](const char c){return !c;}))
        {
            // in POSIX 2001 PAX standard,
            // "At the end of the archive file there shall be two 512-octet logical records filled with binary zeros, interpreted as an end-of-archive indicator."
            // see https://pubs.opengroup.org/onlinepubs/9699919799/utilities/pax.html ustar Interchange Format
            if (last_block_all_zero)
                break;
            last_block_all_zero = true;
            continue;
        }
        last_block_all_zero = false;

        const auto* header = reinterpret_cast<TarFileHeader*>(block);

        if (strncmp(header->ustar.reserved, "ustar  ", 8) == 0)
        {
            standard_ = TarStandard::GNU;
        } else if (strncmp(header->ustar.magic, "ustar", 6) == 0 && strncmp(header->ustar.version, "00", sizeof(header->ustar.version)) == 0)
        {
            standard_ = TarStandard::POSIX_2001_PAX;
        } else
        {
            standard_ = TarStandard::UNKNOWN;
        }

        auto meta = tar_header2file_meta(*header, standard_);

        // in POSIX 2001 PAX standard, if a filename equals "TRAILER!!!", it indicates the end of the archive
        // see https://pubs.opengroup.org/onlinepubs/9699919799/utilities/pax.html cpio Special Entries
        if (standard_ == TarStandard::POSIX_2001_PAX && meta.path == "TRAILER!!!")
        {
            break;
        }
        // 处理长文件名的情况
        // ReSharper disable once CppDFAConstantConditions
        if (previous_special_block)
        {
            // ReSharper disable once CppDFAUnreachableCode
            if (standard_ == TarStandard::GNU)
            {
                if (!long_name.empty())
                {
                    meta.path = long_name;
                }
            } else if (standard_ == TarStandard::POSIX_2001_PAX && !pax_headers.empty())
            {
                if (const auto path = pax_headers.find("path"); path != pax_headers.end())
                {
                    meta.path = path->second;
                }
                if (const auto atime = pax_headers.find("atime"); atime != pax_headers.end())
                {
                    meta.access_time = pax_string2time_point(atime->second);
                }
                if (const auto mtime = pax_headers.find("mtime"); mtime != pax_headers.end())
                {
                    meta.modification_time = pax_string2time_point(mtime->second);
                }
                if (const auto link_path = pax_headers.find("linkpath"); link_path != pax_headers.end() && !link_path->second.empty())
                {
                    meta.symbolic_link_target = link_path->second;
                }
            }
            previous_special_block = false;
        }
        else
        {
            long_name = "";
            pax_headers.clear();
            previous_special_block = false;

            if (standard_ == TarStandard::GNU && long_name.empty())
            {
                if (header->type_flag == 'L')   // 长文件名标示
                {
                    // ReSharper disable once CppDFAUnreachableCode
                    auto name_size = meta.size;
                    TarBlock name_buffer{};
                    while (name_size > 0 && ifs_->read(name_buffer.block, sizeof(name_buffer)))
                    {
                        current_offset += sizeof(name_buffer);
                        long_name.append(name_buffer.block, ifs_->gcount());
                        name_size -= ifs_->gcount();
                    }
                    previous_special_block = true;
                    continue;
                }
                // ReSharper disable once CppDFAUnreachableCode
                if (header->size[0] & 0x80)
                {
                    // GNU 扩展的大文件头
                    size_t b64size = 0;
                    for (int i=0; i<sizeof(header->size); ++i)
                    {
                        b64size = (b64size << 8) | (i ? header->size[i] : (header->size[0] & 0x7F));
                    }
                    meta.size = b64size;
                }
            }
            else if (standard_ == TarStandard::POSIX_2001_PAX)
            {
                if (header->type_flag == 'x')   // PAX 扩展头
                {
                    // ReSharper disable once CppDFAUnreachableCode
                    int pax_size = static_cast<int>(meta.size);
                    pax_size = ((pax_size / TarBlockSize) + static_cast<bool>(pax_size % TarBlockSize)) * TarBlockSize;
                    std::string pax_buffer;
                    pax_buffer.resize(pax_size);
                    ifs_->read(&pax_buffer[0], pax_size);
                    current_offset += pax_size;
                    if (ifs_->gcount() != pax_size)
                    {
                        is_valid_ = false;
                        return;
                    }
                    size_t pos = 0;
                    while (pos < pax_buffer.size())
                    {
                        const size_t next_space = pax_buffer.find(' ', pos);
                        if (next_space == std::string::npos || next_space == pos)
                            break;
                        const size_t record_size = std::stoul(pax_buffer.substr(pos, next_space - pos));
                        if (record_size == 0 || pos + record_size > pax_buffer.size())
                            break;
                        const size_t equal_pos = pax_buffer.find('=', next_space + 1);
                        if (equal_pos == std::string::npos || equal_pos >= pos + record_size)
                            break;
                        std::string key = pax_buffer.substr(next_space + 1, equal_pos - next_space - 1);
                        const std::string value = pax_buffer.substr(equal_pos + 1, pos + record_size - equal_pos - 2); // -2 to remove trailing newline
                        pax_headers[key] = value;
                        pos += record_size;
                    }
                    previous_special_block = true;
                    continue;
                }
            }
        }

        if (!insert_entity(meta, ifs_->tellg()))
        {
            is_valid_ = false;
            return;
        }
        auto size = meta.size;
        while (!ifs_->eof() && size > 0)
        {
            ifs_->read(block, 512);
            current_offset += 512;
            const auto gcount = ifs_->gcount();
            if (gcount != 512)
            {
                is_valid_ = false;
                return;
            }
            if (size <= gcount)
                break;
            size -= gcount;
        }
    }
}

bool TarFile::insert_entity(const FileEntityMeta& meta, const int offset) const
{
    const auto stmt = db_.create_statement("INSERT INTO entity (" + db::TarInitializationStrategy::SQLEntityColumns + ") "
    "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
    int i = 1;
    sqlite3_bind_text(stmt.get(), i++, reinterpret_cast<const char*>(meta.path.generic_u8string().c_str()), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.type));
    sqlite3_bind_int64(stmt.get(), i++, static_cast<sqlite3_int64>(meta.size));
    sqlite3_bind_int(stmt.get(), i++, offset);
    sqlite3_bind_int64(stmt.get(), i++, std::chrono::duration_cast<std::chrono::seconds>(meta.creation_time.time_since_epoch()).count());
    sqlite3_bind_int64(stmt.get(), i++, std::chrono::duration_cast<std::chrono::seconds>(meta.modification_time.time_since_epoch()).count());
    sqlite3_bind_int64(stmt.get(), i++, std::chrono::duration_cast<std::chrono::seconds>(meta.access_time.time_since_epoch()).count());
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.posix_mode));
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.uid));
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.gid));
    sqlite3_bind_text(stmt.get(), i++, meta.user_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), i++, meta.group_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.windows_attributes));
    if (meta.type == FileEntityType::SymbolicLink)
        sqlite3_bind_text(stmt.get(), i++, reinterpret_cast<const char*>(meta.symbolic_link_target.generic_u8string().c_str()), -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt.get(), i++);
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.device_major));
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.device_minor));
    return db_.execute(*stmt, true);
}

FileEntityMeta TarFile::tar_header2file_meta(const TarFileHeader &header, TarStandard standard)
{
    auto path = std::string(header.name);
    auto prefix = std::string(header.prefix);
    if (!prefix.empty() && path.size() >= 99 && prefix.front() != '\0')
    {
        path = prefix + path;
    }
    const size_t size = std::strtoull(header.size, nullptr, 8);
    FileEntityType type;
    switch (header.type_flag)
    {
        case '0': // NOLINT(*-branch-clone)
        case '\0':
            type = FileEntityType::RegularFile;
            break;
        case '1':
            type = FileEntityType::RegularFile; // hard link, treat as regular file
            break;
        case '2':
            type = FileEntityType::SymbolicLink;
            break;
        case '3':
            type = FileEntityType::CharacterDevice;
            break;
        case '4':
            type = FileEntityType::BlockDevice;
            break;
        case '5':
            type = FileEntityType::Directory;
            break;
        case '6':
            type = FileEntityType::Fifo;
            break;
        case '7':
            // in GNU standard it's a reserved value for contiguous file
            // in POSIX 2001 PAX it's a socket, and standard says that
            // "Implementations without such extensions should treat this file as a regular file (type 0)."
            // see https://pubs.opengroup.org/onlinepubs/9699919799/utilities/pax.html
            if (standard == TarStandard::POSIX_2001_PAX)
            {
                type = FileEntityType::RegularFile;
                break;
            }
            // in POSIX 1988 USTAR it's a reserved value, treat as unknown
            [[fallthrough]];
        case 'x':
        case 'X':
        case 'L':
        default:
            type = FileEntityType::Unknown;
            break;
    }
    FileEntityMeta meta = {
        path,
        type,
        size,
        std::chrono::system_clock::from_time_t(std::strtoll(header.mtime, nullptr, 8)),
        std::chrono::system_clock::from_time_t(std::strtoll(header.mtime, nullptr, 8)),
        std::chrono::system_clock::from_time_t(std::strtoll(header.mtime, nullptr, 8)),
        static_cast<uint32_t>(std::strtoul(header.mode, nullptr, 8)),
        static_cast<uint32_t>(std::strtoul(header.uid, nullptr, 8)),
        static_cast<uint32_t>(std::strtoul(header.gid, nullptr, 8)),
        std::string(header.uname, strnlen(header.uname, sizeof(header.uname))),
        std::string(header.gname, strnlen(header.gname, sizeof(header.gname))),
        0,
        (type == FileEntityType::SymbolicLink) ? std::string(header.link_name, strnlen(header.link_name, sizeof(header.link_name))) : "",
        static_cast<uint32_t>(std::strtoul(header.device_major, nullptr, 8)),
        static_cast<uint32_t>(std::strtoul(header.device_minor, nullptr, 8))
    };
    meta.creation_time = meta.access_time = meta.modification_time;
    return meta;
}

// return nullptr if it's not a file, or file not found
std::unique_ptr<TarFile::TarIstream> TarFile::get_file_stream(const std::filesystem::path& path) const
{
    if (!is_valid_ || !ifs_ || !ifs_->is_open())
        return nullptr;
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
        "SELECT " + db::TarInitializationStrategy::SQLEntityColumns + " FROM entity WHERE path = ? LIMIT 1;"
    );

    // reinterpret_cast for C++20
    // ReSharper disable once CppRedundantCastExpression
    sqlite3_bind_text(stmt.get(), 1, reinterpret_cast<const char*>(path_str.c_str()), -1, SQLITE_TRANSIENT);

    std::unique_ptr<TarIstream> tar;
    try
    {
        const auto entity = db_.query_one<db::TarInitializationStrategy::SQLEntity>(std::move(stmt));
        auto [meta, offset] = sql_entity2file_meta(entity);
        char block[512];
        ifs_->seekg(offset, std::ios::beg);
        ifs_->read(block, sizeof(TarFileHeader));
        tar = std::make_unique<TarIstream>(*ifs_.get(), offset, meta);
    } catch ([[maybe_unused]] const std::exception& e)
    {
        return nullptr;
    }
    return tar;
}

bool TarFile::add_entity(ReadableFile& file)
{
    if (!ofs_ || !ofs_->is_open())
        return false;

    // Get the file metadata from ReadableFile
    FileEntityMeta& meta = file.get_meta();
    auto full_path = meta.path.generic_u8string();
    bool is_long_path = false;
    std::unordered_map<std::string, std::string> pax_map;

    // Set default standard if not specified
    if (standard_ == TarStandard::UNKNOWN)
        standard_ = TarStandard::POSIX_2001_PAX; // Default to PAX for better compatibility

    // Handle long filenames based on the standard
    if (standard_ == TarStandard::GNU && full_path.size() >= 99)
    {
        // Create GNU long filename entry
        TarFileHeader long_name_header{};
        std::string long_name_entry_path = "\"" + full_path + "\"";

        // Write long filename header
        // The name field is set to "././@LongLink" for GNU long filename entries
        memset(reinterpret_cast<char*>(&long_name_header), 0, sizeof(long_name_header));
        strncpy_s(long_name_header.name, "././@LongLink", sizeof(long_name_header.name));
        snprintf(long_name_header.size, sizeof(long_name_header.size), "%011o", static_cast<unsigned int>(long_name_entry_path.size()));
        long_name_header.type_flag = 'L'; // GNU long filename marker
        // USTAR magic and version of GNU
        strncpy_s(long_name_header.ustar.reserved, "ustar  ", sizeof(long_name_header.ustar.reserved));
        memset(long_name_header.checksum, ' ', sizeof(long_name_header.checksum));

        // Calculate checksum
        unsigned int checksum = 0;
        const auto bytes = reinterpret_cast<const unsigned char*>(&long_name_header);
        for (size_t i = 0; i < sizeof(TarFileHeader); ++i)
        {
            checksum += bytes[i];
        }
        snprintf(long_name_header.checksum, sizeof(long_name_header.checksum), "%06o", checksum);
        long_name_header.checksum[6] = '\0';
        long_name_header.checksum[7] = ' ';

        // Write long filename header block
        ofs_->write(reinterpret_cast<const char*>(&long_name_header), sizeof(long_name_header));

        // Write long filename content
        size_t padding = ((long_name_entry_path.size() / TarBlockSize) + static_cast<bool>(long_name_entry_path.size() % TarBlockSize)) * TarBlockSize;
        std::vector<char> name_block(padding, 0);
        memcpy(name_block.data(), long_name_entry_path.c_str(), long_name_entry_path.size());
        ofs_->write(name_block.data(), static_cast<long long>(name_block.size()));

        is_long_path = true;
    }
    else if (standard_ == TarStandard::POSIX_2001_PAX && full_path.size() >= 253)
    {
        // Create PAX extended header for long filename
        std::string pax_content = " path=" + full_path + "\n";
        int pax_len_len = 3;    // path that need to store in extra header must be at least 3 digits.
        size_t pax_content_size = pax_content.size(), base10 = 1000;
        while (pax_len_len + pax_content_size >= base10)
        {
            pax_len_len++;
            base10 *= 10;
        }
        std::string pax_header = std::to_string(pax_len_len+pax_content_size) + pax_content;
        pax_map["path"] = full_path;

        // Create PAX header entry
        TarFileHeader pax_header_entry{};
        // The name field is set to "PaxHeader/@PaxHeader" for PAX extended header entries
        memset(reinterpret_cast<char*>(&pax_header_entry), 0, sizeof(pax_header_entry));
        strncpy_s(pax_header_entry.name, "PaxHeader/@PaxHeader", sizeof(pax_header_entry.name));
        snprintf(pax_header_entry.size, sizeof(pax_header_entry.size), "%011o", static_cast<unsigned int>(pax_header.size()));
        pax_header_entry.type_flag = 'x'; // PAX extended header marker
        // USTAR magic and version
        strncpy_s(pax_header_entry.ustar.magic, "ustar", sizeof(pax_header_entry.ustar.magic));
        pax_header_entry.ustar.version[0] = pax_header_entry.ustar.version[1] = '0';
        memset(pax_header_entry.checksum, ' ', sizeof(pax_header_entry.checksum));

        // Calculate checksum
        unsigned int checksum = 0;
        const auto bytes = reinterpret_cast<const unsigned char*>(&pax_header_entry);
        for (size_t i = 0; i < sizeof(TarFileHeader); ++i)
        {
            checksum += bytes[i];
        }
        snprintf(pax_header_entry.checksum, sizeof(pax_header_entry.checksum), "%06o", checksum);
        pax_header_entry.checksum[6] = '\0';
        pax_header_entry.checksum[7] = ' ';

        // Write PAX header block
        // ofs_->write(reinterpret_cast<const char*>(&pax_header_entry), sizeof(pax_header_entry));

        // Write PAX header content
        size_t padding = ((pax_header.size() / TarBlockSize) + static_cast<bool>(pax_header.size() % TarBlockSize)) * TarBlockSize;
        std::vector<char> pax_block(padding, 0);
        memcpy(pax_block.data(), pax_header.c_str(), pax_header.size());
        // ofs_->write(pax_block.data(), pax_block.size());

        is_long_path = true;
    } else if (standard_ == TarStandard::POSIX_1988_USTAR && full_path.size() >= 256)
    {
        full_path = full_path.substr(0, 255); // Truncate to fit USTAR limits
        printf("WARNING! File path too long for USTAR format, truncated to: %s\n", full_path.c_str());

        // Use default behavior
        is_long_path = false;
    }

    // 更新PAX扩展字段
    if (standard_ == TarStandard::POSIX_2001_PAX)
    {
        if (meta.access_time != meta.creation_time)
        {
            pax_map["atime"] = time_point2pax_string(meta.access_time);
        }
        if (meta.modification_time != meta.creation_time)
        {
            pax_map["mtime"] = time_point2pax_string(meta.modification_time);
        }
        if (const auto link_path = meta.symbolic_link_target.generic_u8string(); link_path.length() >= 100)
        {
            pax_map["linkpath"] = link_path;
        }
        if (!meta.user_name.empty())
        {
            pax_map["uname"] = meta.user_name;
        }
        if (!meta.group_name.empty())
        {
            pax_map["gname"] = meta.group_name;
        }
    }

    // generate a shortcut for long path and replace it (long path SHOULD be handled above)
    if (is_long_path)
    {
        // shortcut is like '@PathCut/_pc_crc32/01234567(CRC32)/filename'
        // shortcut without filename will take 29 bytes(@PathCut/_pc_crc32/01234567/\0)
        const auto crc32 = crc::crc32(reinterpret_cast<const char*>(full_path.c_str()), full_path.size());
        std::stringstream new_path_ss;
        new_path_ss << "@PathCut/_pc_crc32/";
        new_path_ss << std::uppercase << std::setw(8) << std::setfill('0') << std::hex << crc32 << "/";
        if (auto filename = std::filesystem::path(full_path).filename().generic_u8string();
            filename.size() >= 100 - 29)
        {
            new_path_ss << filename.substr(filename.size() - (100 - 29 - 1)); // -1 for null terminator
        } else
        {
            new_path_ss << filename;
        }
        meta.path = new_path_ss.str();
    }

    // 写入PAX扩展字段
    if (standard_ == TarStandard::POSIX_2001_PAX && !pax_map.empty())
    {
        std::stringstream ss;
        for (const auto&[key, value]: pax_map)
        {
            ss << key_value2pax_field(key, value);
        }
        const std::string pax_header = ss.str();
        // Create PAX header entry
        TarFileHeader pax_header_entry{};
        // The name field is set to "PaxHeader/@PaxHeader" for PAX extended header entries
        memset(reinterpret_cast<char*>(&pax_header_entry), 0, sizeof(pax_header_entry));
        strncpy_s(pax_header_entry.name, "PaxHeader/@PaxHeader", sizeof(pax_header_entry.name));
        snprintf(pax_header_entry.size, sizeof(pax_header_entry.size), "%011o", static_cast<unsigned int>(pax_header.size()));
        pax_header_entry.type_flag = 'x'; // PAX extended header marker
        // USTAR magic and version
        strncpy_s(pax_header_entry.ustar.magic, "ustar", sizeof(pax_header_entry.ustar.magic));
        pax_header_entry.ustar.version[0] = pax_header_entry.ustar.version[1] = '0';
        memset(pax_header_entry.checksum, ' ', sizeof(pax_header_entry.checksum));

        // Calculate checksum
        unsigned int checksum = 0;
        const auto bytes = reinterpret_cast<const unsigned char*>(&pax_header_entry);
        for (size_t i = 0; i < sizeof(TarFileHeader); ++i)
        {
            checksum += bytes[i];
        }
        snprintf(pax_header_entry.checksum, sizeof(pax_header_entry.checksum), "%06o", checksum);
        pax_header_entry.checksum[6] = '\0';
        pax_header_entry.checksum[7] = ' ';

        // Write PAX header block
        ofs_->write(reinterpret_cast<const char*>(&pax_header_entry), sizeof(pax_header_entry));

        // Write PAX header content
        size_t padding = ((pax_header.size() / TarBlockSize) + static_cast<bool>(pax_header.size() % TarBlockSize)) * TarBlockSize;
        std::vector<char> pax_block(padding, 0);
        memcpy(pax_block.data(), pax_header.c_str(), pax_header.size());
        ofs_->write(pax_block.data(), static_cast<long long>(pax_block.size()));
    }

    // Get the current offset for the entry
    int entry_offset = static_cast<int>(ofs_->tellp());

    // Generate the main tar header
    TarFileHeader header = file_meta2tar_header(meta, standard_);

    // Write the tar header
    ofs_->write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write the file content if it's a regular file
    if (meta.type == FileEntityType::RegularFile)
    {
        size_t remaining = meta.size;

        while (remaining > 0)
        {
            constexpr size_t buffer_size = 8192;
            size_t to_read = std::min(buffer_size, remaining);
            auto buffer = file.read(to_read);
            if (!buffer || buffer->empty())
                break;

            ofs_->write(reinterpret_cast<const char*>(buffer->data()), static_cast<long long>(buffer->size()));
            remaining -= buffer->size();
        }

        // Pad to 512-byte boundary
        size_t padding = (TarBlockSize - (meta.size % TarBlockSize)) % TarBlockSize;
        if (padding > 0)
        {
            std::vector<char> pad_block(padding, 0);
            ofs_->write(pad_block.data(), static_cast<long long>(pad_block.size()));
        }
    }
    else if (meta.type == FileEntityType::Directory)
    {
        // Directories are empty but need to be padded to 512-byte boundary
        // No content to write, just ensure we're at the correct offset
    }

    // Insert the entity into the database
    return insert_entity(meta, entry_offset);
}

void TarFile::close()
{
    if (ifs_)
    {
        if (ifs_->is_open()) ifs_->close();
        is_valid_ = false;
        return;
    }
    if (!ofs_ || !ofs_->is_open())
    {
        is_valid_ = false;
        return;
    }

    // Write two empty blocks to mark the end of archive (in PAX extension)
    if (standard_ == TarStandard::POSIX_2001_PAX)
    {
        TarBlock empty_block{};
        memset(empty_block.block, 0, sizeof(empty_block.block));

        // Write first empty block
        ofs_->write(empty_block.block, sizeof(empty_block.block));

        // Write second empty block
        ofs_->write(empty_block.block, sizeof(empty_block.block));
    }

    // Close the output file stream
    ofs_->close();
}

TarFileHeader TarFile::file_meta2tar_header(const FileEntityMeta &meta, const TarStandard standard)
{
    TarFileHeader header{};
    auto full_path = meta.path.generic_u8string();
    if (meta.type == FileEntityType::Directory && !full_path.empty() && full_path.back() != '/')
    {
        full_path += '/';
    }
    // prefix and name should end with \0
    if (full_path.size() >= 253)
        full_path = full_path.substr(0, 253);
    if (full_path.size() >= 100)
    {
        // For ustar format, split into prefix and name
        const std::string prefix = full_path.substr(0, full_path.size() - 99);
        const std::string name = full_path.substr(full_path.size() - 99);
        strncpy_s(header.prefix, prefix.c_str(), sizeof(header.prefix));
        strncpy_s(header.name, name.c_str(), sizeof(header.name));
    }
    else
    {
        strncpy_s(header.name, reinterpret_cast<const char*>(full_path.c_str()), sizeof(header.name));
    }
    if (auto link_path = meta.symbolic_link_target.generic_u8string(); !meta.symbolic_link_target.empty())
    {
        if (link_path.length() > 100)
        {
            link_path = link_path.substr(0, 100);
        }
        strncpy_s(header.link_name, link_path.c_str(), sizeof(header.link_name));
    }

    // Set all metadata fields
    snprintf(header.mode, sizeof(header.mode), "%07o", meta.posix_mode);
    snprintf(header.uid, sizeof(header.uid), "%07o", meta.uid);
    snprintf(header.gid, sizeof(header.gid), "%07o", meta.gid);
    snprintf(header.size, sizeof(header.size), "%011o", static_cast<unsigned int>(meta.type == FileEntityType::Directory ? 0 : meta.size));

    // Set mtime based on modification time
    const auto mtime = std::chrono::duration_cast<std::chrono::seconds>(meta.modification_time.time_since_epoch()).count();
    snprintf(header.mtime, sizeof(header.mtime), "%011o", static_cast<unsigned int>(mtime));
    memset(header.checksum, ' ', sizeof(header.checksum));
    switch (meta.type)
    {
        case FileEntityType::RegularFile:
            header.type_flag = '0';
            break;
        case FileEntityType::SymbolicLink:
            header.type_flag = '2';
            strncpy_s(header.link_name, reinterpret_cast<const char*>(meta.symbolic_link_target.generic_u8string().c_str()), sizeof(header.link_name));
            break;
        case FileEntityType::Directory:
            header.type_flag = '5';
            break;
        case FileEntityType::CharacterDevice:
            header.type_flag = '3';
            break;
        case FileEntityType::BlockDevice:
            header.type_flag = '4';
            break;
        case FileEntityType::Fifo:
            header.type_flag = '6';
            break;
        case FileEntityType::Socket:
            header.type_flag = '7';
            break;
        default:
            header.type_flag = '0';
            break;
    }

    // Set device information for device files
    snprintf(header.device_major, sizeof(header.device_major), "%07o", meta.device_major);
    snprintf(header.device_minor, sizeof(header.device_minor), "%07o", meta.device_minor);

    // Set user and group names
    strncpy_s(header.uname, meta.user_name.c_str(), sizeof(header.uname));
    strncpy_s(header.gname, meta.group_name.c_str(), sizeof(header.gname));

    // Set ustar format information based on the standard
    if (standard == TarStandard::GNU)
        strncpy_s(header.ustar.reserved, "ustar  ", sizeof(header.ustar.reserved));
    else if (standard == TarStandard::POSIX_2001_PAX) {
        strncpy_s(header.ustar.magic, "ustar", sizeof(header.ustar.magic));
        header.ustar.version[0] = header.ustar.version[1] = '0';
    } else
        memset(header.ustar.reserved, 0, sizeof(header.ustar));

    // Calculate checksum
    unsigned int checksum = 0;
    const auto bytes = reinterpret_cast<const unsigned char*>(&header);
    for (size_t i = 0; i < sizeof(TarFileHeader); ++i)
    {
        checksum += bytes[i];
    }
    snprintf(header.checksum, sizeof(header.checksum), "%06o", checksum);
    header.checksum[6] = '\0';
    header.checksum[7] = ' ';

    return header;
}

std::pair<FileEntityMeta, int> TarFile::sql_entity2file_meta(const db::TarInitializationStrategy::SQLEntity& entity)
{
    auto [file_path, type, size, offset, ctime, mtime, atime, posix_mode, uid, gid,
        user_name, group_name, windows_attributes, symbolic_link_target, device_major, device_minor] = entity;
    return std::make_pair(FileEntityMeta({
        file_path,
        static_cast<FileEntityType>(type),
        static_cast<size_t>(size),
        std::chrono::system_clock::from_time_t(ctime),
        std::chrono::system_clock::from_time_t(mtime),
        std::chrono::system_clock::from_time_t(atime),
        static_cast<uint32_t>(posix_mode),
        static_cast<uint32_t>(uid),
        static_cast<uint32_t>(gid),
        user_name,
        group_name,
        static_cast<uint32_t>(windows_attributes),
        symbolic_link_target,
        static_cast<uint32_t>(device_major),
        static_cast<uint32_t>(device_minor)
    }), offset);
}

std::vector<std::pair<FileEntityMeta, int>> TarFile::list_dir(const std::filesystem::path& path) const
{
    // path like "/...", and not contain any driver letter
    if (path.has_root_name() || !path.is_relative())
        return {}; // cannot analyze a path starts with "C:\"
    auto stmt = db_.create_statement(
        "SELECT " + db::TarInitializationStrategy::SQLEntityColumns + " FROM entity WHERE path LIKE ? ORDER BY path ASC;"
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

    std::vector<std::pair<FileEntityMeta, int>> results;
    auto rs = db_.query<db::TarInitializationStrategy::SQLEntity>(std::move(stmt));
    for (const auto& entity : rs)
    {
        auto [meta, offset] = sql_entity2file_meta(entity);
        results.emplace_back(meta, offset);
    }
    return results;
}

TarFile::~TarFile()
{
    try {
        close();
    } catch (...) {
        // Suppress exceptions during destruction
    }
}
