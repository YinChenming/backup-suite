//
// Created by BackupSuite Database Strategies
//

#ifndef BACKUPSUITE_DATABASE_STRATEGIES_H
#define BACKUPSUITE_DATABASE_STRATEGIES_H

#include <string>
#include "utils/database.h"

namespace db {

    // TAR文件数据库初始化策略
class BACKUP_SUITE_API TarInitializationStrategy final : public db::DatabaseInitializationStrategy
    {
    public: using SQLEntity = std::tuple<
            std::string,    // path
            int,            // type
            int,            // size
            int,            // offset
            long long,      // creation_time
            long long,      // modification_time
            long long,      // access_time
            int,            // posix_mode
            int,            // uid
            int,            // gid
            std::string,    // user_name
            std::string,    // group_name
            int,            // windows_attributes
            std::string,    // symbolic_link_target
            int,            // device_major
            int             // device_minor
        >;
        inline const static std::string SQLEntityColumns = (
            " path, type, size, offset, creation_time, modification_time, "
            "access_time, posix_mode, uid, gid, user_name, group_name, "
            "windows_attributes, symbolic_link_target, device_major, "
            "device_minor "
        );
    protected:
        [[nodiscard]] std::string get_initialization_sql() const override
        {
            return (
                "CREATE TABLE IF NOT EXISTS entity_type("
                "id INTEGER PRIMARY KEY UNIQUE,"
                "name TEXT NOT NULL UNIQUE"
                ");"

                "INSERT INTO entity_type (id, name) VALUES "
                "(0, 'unknown'),"
                "(1, 'regular_file'),"
                "(2, 'directory'),"
                "(4, 'symbolic_link'),"
                "(8, 'fifo'),"
                "(16, 'socket'),"
                "(32, 'block_device'),"
                "(64, 'character_device')"
                ";"

                "CREATE TABLE IF NOT EXISTS entity("
                "id INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE,"
                "path TEXT NOT NULL,"
                "type INTEGER NOT NULL DEFAULT 0,"
                "size INTEGER NOT NULL,"
                "offset INTEGER NOT NULL DEFAULT 0,"
                "creation_time DATETIME NOT NULL,"
                "modification_time DATETIME NOT NULL,"
                "access_time DATETIME NOT NULL,"
                "posix_mode INTEGER NOT NULL DEFAULT 0,"
                "uid INTEGER NOT NULL DEFAULT 0,"
                "gid INTEGER NOT NULL DEFAULT 0,"
                "user_name TEXT NOT NULL DEFAULT '',"
                "group_name TEXT NOT NULL DEFAULT '',"
                "windows_attributes INTEGER NOT NULL DEFAULT 0,"
                "symbolic_link_target TEXT,"
                "device_major INTEGER DEFAULT 0,"
                "device_minor INTEGER DEFAULT 0,"
                "FOREIGN KEY(type) REFERENCES entity_type(id) ON DELETE CASCADE"
                ");"
            );
        }
    };

    // ZIP文件数据库初始化策略
    class ZipInitializationStrategy : public DatabaseInitializationStrategy {
    public:
        using SQLZipEntity = std::tuple<
            uint8_t,            // version_made_by
            uint16_t,            // version_needed
            uint16_t,            // general_purpose
            uint16_t,            // compression_method
            long long,           // last_modified
            uint32_t,            // crc32
            uint32_t,            // compressed_size
            uint32_t,            // uncompressed_size
            uint16_t,            // disk_number
            uint16_t,            // internal_attributes
            uint32_t,            // external_attributes
            uint32_t,            // local_header_offset
            std::string,         // filename
            std::vector<uint8_t>,// extra_field
            std::string          // file_comment
        >;

        inline const static std::string SQLEntityColumns = (
            " version_made_by, version_needed, general_purpose, compression_method, "
            "last_modified, crc32, compressed_size, uncompressed_size, disk_number, "
            "internal_attributes, external_attributes, local_header_offset, "
            "filename, extra_field, file_comment "
        );

        [[nodiscard]] std::string get_initialization_sql() const override {
            return (
                "CREATE TABLE IF NOT EXISTS zip_entity("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "version_made_by INTEGER NOT NULL,"
                "version_needed INTEGER DEFAULT 10,"
                "general_purpose INTEGER DEFAULT 0,"
                "compression_method INTEGER DEFAULT 0,"
                "last_modified DATETIME DEFAULT '1970-01-01 00:00:00',"
                "crc32 INTEGER DEFAULT 0,"
                "compressed_size INTEGER DEFAULT 0,"
                "uncompressed_size INTEGER DEFAULT 0,"
                "disk_number INTEGER DEFAULT 0,"
                "internal_attributes INTEGER DEFAULT 0,"
                "external_attributes INTEGER DEFAULT 0,"
                "local_header_offset INTEGER DEFAULT 0,"

                "filename TEXT NOT NULL DEFAULT '',"
                "extra_field BLOB,"
                "file_comment TEXT DEFAULT ''"
                ");"
            );
        }
    };

} // namespace db

#endif // BACKUPSUITE_DATABASE_STRATEGIES_H
