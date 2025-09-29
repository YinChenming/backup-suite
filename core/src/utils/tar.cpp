//
// Created by ycm on 25-9-5.
//
#include "utils/tar.h"

#include <fstream>

using namespace tar;

void TarFile::init_db_from_tar()
{
    if (!ifs_ || !ifs_->is_open())
    {
        return;
    }
    char block[512];
    while (ifs_->read(block, 512))
    {
        if (ifs_->gcount() != 512)
        {
            is_valid_ = false;
            return;
        }
        if (std::all_of(std::begin(block), std::end(block), [](char c){return !c;}))
        {
            // Bad header block! all value are 0
            continue;
        }

        const auto* header = reinterpret_cast<TarFileHeader*>(block);

        if (strncmp(header->ustar.reserved, "ustar  ", 8) == 0)
        {
            standard_ = TarStandard::GNU;
        } else if (strncmp(header->ustar.magic, "ustar", 6) == 0 && strncmp(header->ustar.version, "00", sizeof(header->ustar.version)) == 0)
        {
            standard_ = TarStandard::POSIX_1988_USTAR;
        } else
        {
            standard_ = TarStandard::UNKNOWN;
        }

        auto meta = tar_header2file_meta(*header);

        if (!insert_entity(meta, static_cast<int>(ifs_->tellg())))
        {
            is_valid_ = false;
            return;
        }
        auto size = meta.size;
        while (!ifs_->eof() && size > 0)
        {
            ifs_->read(block, 512);
            const auto gcount = ifs_->gcount();
            if (size <= gcount)
                break;
            size -= gcount;
        }
    }
}

bool TarFile::insert_entity(const FileEntityMeta& meta, const int offset) const
{
    auto stmt = db_.create_statement("INSERT INTO entity (path, type, size, offset, creation_time, modification_time, access_time, posix_mode, uid, gid, user_name, group_name, windows_attributes, symbolic_link_target, device_major, device_minor) "
    "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
    int i = 1;
    sqlite3_bind_text(stmt.get(), i++, meta.path.string().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.type));
    sqlite3_bind_int64(stmt.get(), i++, static_cast<sqlite3_int64>(meta.size));
    sqlite3_bind_int(stmt.get(), i++, offset);
    sqlite3_bind_int64(stmt.get(), i++, static_cast<sqlite3_int64>(std::chrono::duration_cast<std::chrono::seconds>(meta.creation_time.time_since_epoch()).count()));
    sqlite3_bind_int64(stmt.get(), i++, static_cast<sqlite3_int64>(std::chrono::duration_cast<std::chrono::seconds>(meta.modification_time.time_since_epoch()).count()));
    sqlite3_bind_int64(stmt.get(), i++, static_cast<sqlite3_int64>(std::chrono::duration_cast<std::chrono::seconds>(meta.access_time.time_since_epoch()).count()));
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.posix_mode));
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.uid));
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.gid));
    sqlite3_bind_text(stmt.get(), i++, meta.user_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), i++, meta.group_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.windows_attributes));
    if (meta.type == FileEntityType::SymbolicLink)
        sqlite3_bind_text(stmt.get(), i++, meta.symbolic_link_target.string().c_str(), -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt.get(), i++);
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.device_major));
    sqlite3_bind_int(stmt.get(), i++, static_cast<int>(meta.device_minor));
    return db_.execute(*stmt.get(), true);
}

FileEntityMeta TarFile::tar_header2file_meta(const TarFileHeader &header)
{
    auto path = std::string(header.name, strnlen(header.name, sizeof(header.name)));
    auto prefix = std::string(header.prefix, sizeof(header.prefix));
    if (!prefix.length())
    {
        path = prefix + "/" + path;
    }
    const size_t size = std::strtoull(header.size, nullptr, 8);
    FileEntityType type;
    switch (header.type_flag)
    {
        case '0':
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
        case '7':   // in GNU standard it's a reserved value for contiguous file
        default:
            type = FileEntityType::Unknown;
            break;
    }
    FileEntityMeta meta = {
        path,
        type,
        size,
        std::chrono::system_clock::from_time_t(std::strtoull(header.mtime, nullptr, 8)),
        std::chrono::system_clock::from_time_t(std::strtoull(header.mtime, nullptr, 8)),
        std::chrono::system_clock::from_time_t(std::strtoull(header.mtime, nullptr, 8)),
        std::strtoul(header.mode, nullptr, 8),
        std::strtoul(header.uid, nullptr, 8),
        std::strtoul(header.gid, nullptr, 8),
        std::string(header.uname, strnlen(header.uname, sizeof(header.uname))),
        std::string(header.gname, strnlen(header.gname, sizeof(header.gname))),
        0,
        (type == FileEntityType::SymbolicLink) ? std::string(header.link_name, strnlen(header.link_name, sizeof(header.link_name))) : "",
        std::strtoul(header.device_major, nullptr, 8),
        std::strtoul(header.device_minor, nullptr, 8)
    };
    meta.creation_time = meta.access_time = meta.modification_time;
    return meta;
}

TarFileHeader TarFile::file_meta2tar_header(const FileEntityMeta &meta, const TarStandard standard)
{
    TarFileHeader header{};
    std::string full_path = meta.path.string();
    if (full_path.size() > 255)
        full_path = full_path.substr(0, 255);
    if (full_path.size() > 100)
    {
        std::string prefix = full_path.substr(0, full_path.size() - 100);
        std::string name = full_path.substr(full_path.size() - 100);
        strncpy_s(header.prefix, prefix.c_str(), sizeof(header.prefix));
        strncpy_s(header.name, name.c_str(), sizeof(header.name));
    }
    else
    {
        strncpy_s(header.name, full_path.c_str(), sizeof(header.name));
    }
    snprintf(header.mode, sizeof(header.mode), "%07o", meta.posix_mode);
    snprintf(header.uid, sizeof(header.uid), "%07o", meta.uid);
    snprintf(header.gid, sizeof(header.gid), "%07o", meta.gid);
    snprintf(header.size, sizeof(header.size), "%011o", static_cast<unsigned int>(meta.size));
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
            strncpy_s(header.link_name, meta.symbolic_link_target.string().c_str(), sizeof(header.link_name));
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
    snprintf(header.device_major, sizeof(header.device_major), "%07o", meta.device_major);
    snprintf(header.device_minor, sizeof(header.device_minor), "%07o", meta.device_minor);
    unsigned int checksum = 0;
    const auto bytes = reinterpret_cast<const unsigned char*>(&header);
    for (size_t i = 0; i < sizeof(TarFileHeader); ++i)
    {
        checksum += bytes[i];
    }
    snprintf(header.checksum, sizeof(header.checksum), "%06o", checksum);
    header.checksum[6] = '\0';
    header.checksum[7] = ' ';

    if (standard == TarStandard::GNU)
        strncpy_s(header.ustar.reserved, "ustar  ", sizeof(header.ustar.reserved));
    else if (standard == TarStandard::POSIX_1988_USTAR)
        strncpy_s(header.ustar.magic, "ustar", sizeof(header.ustar.magic));
    else
        memset(header.ustar.reserved, 0, sizeof(header.ustar));
    strncpy_s(header.uname, meta.user_name.c_str(), sizeof(header.uname));
    strncpy_s(header.gname, meta.group_name.c_str(), sizeof(header.gname));
    return header;
}

// return nullptr if it's not a file, or file not found
std::unique_ptr<TarFile::TarIstream> TarFile::get_file_stream(const std::filesystem::path& path) const
{
    // path like "/...", and not contain any driver letter
    if (path.has_root_name() || !path.is_relative())
        return nullptr; // cannot analyze a path starts with "C:\"
    auto stmt = db_.create_statement(
        "SELECT path, type, size, offset, creation_time, modification_time, "
        "access_time, posix_mode, uid, gid, user_name, group_name, windows_attributes, "
        "symbolic_link_target, device_major, device_minor FROM entity WHERE path = ?;"
    );
    using EntityType = std::tuple<
        std::string, int, int, int, long long, long long, long long, int, int, int,
    std::string, std::string, int, std::string, int, int>;
    sqlite3_bind_text(stmt.get(), 1, path.string().substr(2).c_str(), -1, nullptr);
    std::unique_ptr<TarIstream> tar;
    try
    {
        const auto entity = db_.query_one<EntityType>(*stmt.get());
        auto [meta, offset] = sql_entity2file_meta(entity);
        tar = std::make_unique<TarIstream>(*ifs_.get(), offset, meta);
    } catch (const std::exception& e)
    {
        return nullptr;
    }
    return tar;
}

std::pair<FileEntityMeta, int> TarFile::sql_entity2file_meta(const SQLEntity& entity)
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
        "SELECT path, type, size, offset, creation_time, modification_time, "
        "access_time, posix_mode, uid, gid, user_name, group_name, windows_attributes, "
        "symbolic_link_target, device_major, device_minor FROM entity WHERE path LIKE ?;"
    );
    std::string like_path = path.string().substr(1);
    if (like_path == "." || !like_path.length())
        like_path = "/%";
    else if (like_path.back() == '/')
        like_path += "%";
    else
        like_path += "/%";
    if (like_path.front() == '/')
        like_path = like_path.substr(1);
    sqlite3_bind_text(stmt.get(), 1, like_path.c_str(), -1, nullptr);
    std::vector<std::pair<FileEntityMeta, int>> results;
    auto rs = db_.query<SQLEntity>(std::move(stmt));
    for (const auto& entity : rs)
    {
        auto [meta, offset] = sql_entity2file_meta(entity);
        results.emplace_back(meta, offset);
    }
    return results;
}
