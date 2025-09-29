//
// Created by ycm on 2025/9/24.
//

#ifndef BACKUPSUITE_DATABASE_H
#define BACKUPSUITE_DATABASE_H

#include <memory>
#include <sqlite3.h>
#include <stdexcept>
#include <string>

#include "api.h"

namespace db
{
    struct DatabaseInitializationStrategy
    {
        virtual ~DatabaseInitializationStrategy() = default;
        virtual std::string get_initialization_sql() const{return "";};
    };

    template <typename T>
    T get_column_value(sqlite3_stmt* stmt, int col);

    template<>
    inline int get_column_value<int>(sqlite3_stmt* stmt, int col)
    {
        return sqlite3_column_int(stmt, col);
    }

    template<>
    inline long long get_column_value<long long>(sqlite3_stmt* stmt, int col)
    {
        return sqlite3_column_int64(stmt, col);
    }

    template<>
    inline double get_column_value<double>(sqlite3_stmt* stmt, int col)
    {
        return sqlite3_column_double(stmt, col);
    }

    template<>
    inline std::string get_column_value<std::string>(sqlite3_stmt* stmt, int col)
    {
        const unsigned char* text = sqlite3_column_text(stmt, col);
        return text ? reinterpret_cast<const char*>(text) : "";
    }

    template<typename T>
    struct RowMapper;

    template<typename... Args>
    struct RowMapper<std::tuple<Args...>>
    {
        template<std::size_t... Is>
        static std::tuple<Args...> map_impl(sqlite3_stmt* stmt, std::index_sequence<Is...>)
        {
            return std::make_tuple(get_column_value<Args>(stmt, Is)...);
        }

        static std::tuple<Args...> map(sqlite3_stmt* stmt)
        {
            return map_impl(stmt, std::make_index_sequence<sizeof...(Args)>());
        }
    };


    class BACKUP_SUITE_API Database
    {
        struct SqliteDeleter
        {
            void operator()(sqlite3* p) const
            {
                if (p)
                    sqlite3_close_v2(p);
            }
        };
        struct StatementDeleter
        {
            void operator()(sqlite3_stmt* p) const
            {
                if (p)
                    sqlite3_finalize(p);
            }
        };
        using stmtPointer = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

        std::unique_ptr<sqlite3, SqliteDeleter> db_handle_ = nullptr;

    public:
        template<typename T>
        class ResultSetIterator
        {
        protected:
            sqlite3_stmt* stmt_ = nullptr;
            bool is_done_ = false;

            void advance()
            {
                if (stmt_)
                {
                    int rc = sqlite3_step(stmt_);
                    is_done_ = (rc != SQLITE_ROW);
                    if (rc != SQLITE_ROW && rc != SQLITE_DONE)
                    {
                        throw std::runtime_error("failed to step statement");
                    }
                    if (is_done_)
                    {
                        stmt_ = nullptr;
                    }
                }
            }
        public:
            ResultSetIterator(): stmt_(nullptr), is_done_(true)
            {
            }
            explicit ResultSetIterator(sqlite3_stmt* stmt): stmt_(stmt), is_done_(!stmt)
            {
                if (stmt_)
                if (const auto rc = sqlite3_step(stmt_); rc != SQLITE_ROW)
                {
                    stmt_ = nullptr;
                    is_done_ = true;
                }
            }
            ~ResultSetIterator() = default;
            ResultSetIterator& operator++()
            {
                advance();
                return *this;
            }
            T operator*() const
            {
                return RowMapper<T>::map(stmt_);
            }

            bool operator!=(const ResultSetIterator& other) const
            {
                return is_done_ != other.is_done_ || stmt_ != other.stmt_;
            }
        };

        template<typename T>
        class ResultSet
        {
        protected:
            std::unique_ptr<sqlite3_stmt, StatementDeleter> stmt = nullptr;
        public:
            ResultSet(){}
            explicit ResultSet(std::unique_ptr<sqlite3_stmt, StatementDeleter> stmt) : stmt(std::move(stmt)) {}
            ~ResultSet() = default;

            [[nodiscard]] ResultSetIterator<T> begin()
            {
                if (stmt != nullptr)
                    return ResultSetIterator<T>(stmt.get());
                return ResultSetIterator<T>();
            }
            [[nodiscard]] ResultSetIterator<T> end()
            {
                return ResultSetIterator<T>();
            }
        };


    protected:
        void  initialize(const DatabaseInitializationStrategy &strategy) const;
    public:
        explicit Database(DatabaseInitializationStrategy *strategy = nullptr)
            : Database(":memory:", strategy) { }
        explicit Database(const std::string& dbPath, DatabaseInitializationStrategy *strategy = nullptr)
        {
            sqlite3 *conn;
            if (const int rc = sqlite3_open(dbPath.c_str(), &conn); rc != SQLITE_OK)
            {
                db_handle_ = nullptr;
            } else
            {
                db_handle_ = std::unique_ptr<sqlite3,SqliteDeleter>(conn);
            }

            if (!is_initialized() && strategy)
                initialize(*strategy);
        }
        Database(const Database&) = delete;
        Database& operator=(const Database&) = delete;
        virtual ~Database()
        {
            db_handle_.release();
        }
        [[nodiscard]] bool is_open() const { return db_handle_ != nullptr; }
        [[nodiscard]] bool is_initialized() const;
        [[nodiscard]] bool exec(const std::string& sql, const bool commit = false) const;
        [[nodiscard]] bool execute(const sqlite3_stmt& stmt, const bool commit = false) const;
        template<typename T>
        ResultSet<T> query(const std::string& sql) const
        {
            if (!is_open())
                throw std::runtime_error("database is not open");
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db_handle_.get(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            {
                throw std::runtime_error("failed to prepare statement: " + std::string(sqlite3_errmsg(db_handle_.get())));
            }
            return ResultSet<T>(std::unique_ptr<sqlite3_stmt, StatementDeleter>(stmt));
        }
        template<typename T>
        ResultSet<T> query(stmtPointer stmt) const
        {
            if (!is_open())
                throw std::runtime_error("database is not open");
            if (!stmt)
                return ResultSet<T>(nullptr);
            return ResultSet<T>(std::move(stmt));
        }
        template<typename T>
        T query_one(const std::string& sql) const
        {
            auto rs = query<T>(sql);
            auto it = rs.begin();
            if (it != rs.end())
                return *it;
            throw std::runtime_error("no rows returned");
        }
        template<typename T>
        T query_one(sqlite3_stmt& stmt) const
        {
            if (!is_open())
                throw std::runtime_error("database is not open");
            sqlite3_stmt* s = nullptr;
            if (sqlite3_prepare_v2(db_handle_.get(), sqlite3_sql(&stmt), -1, &s, nullptr) != SQLITE_OK)
            {
                throw std::runtime_error("failed to prepare statement: " + std::string(sqlite3_errmsg(db_handle_.get())));
            }
            auto rs = ResultSet<T>(std::unique_ptr<sqlite3_stmt, StatementDeleter>(s));
            auto it = rs.begin();
            if (it != rs.end())
                return *it;
            throw std::runtime_error("no rows returned");
        }
        [[nodiscard]] std::unique_ptr<sqlite3_stmt, StatementDeleter> create_statement(const std::string& sql) const
        {
            if (!is_open())
                throw std::runtime_error("database is not open");
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db_handle_.get(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            {
                throw std::runtime_error("failed to prepare statement: " + std::string(sqlite3_errmsg(db_handle_.get())));
            }
            return std::unique_ptr<sqlite3_stmt, StatementDeleter>(stmt);
        }
    };

    class BACKUP_SUITE_API DeviceDatabaseInitializationStrategy: public DatabaseInitializationStrategy
    {
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
}
#endif // BACKUPSUITE_DATABASE_H
