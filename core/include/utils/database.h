//
// Created by ycm on 2025/9/24.
//

#ifndef BACKUPSUITE_DATABASE_H
#define BACKUPSUITE_DATABASE_H

#include <memory>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <type_traits>

#include "api.h"

namespace db
{
    struct BACKUP_SUITE_API DatabaseInitializationStrategy
    {
        virtual ~DatabaseInitializationStrategy() = default;
        [[nodiscard]] virtual std::string get_initialization_sql() const{return "";};
    };

    // 1. 基础模板：默认情况下任何类型都不是 vector
    template <typename T>
    struct is_vector : std::false_type {};
    // 2. 偏特化模板：匹配 std::vector 结构
    // 注意：vector 有两个模板参数（类型 T 和 分配器 Allocator），
    // 必须同时写出这两个参数才能正确匹配所有 vector 类型。
    template <typename T, typename Allocator>
    struct is_vector<std::vector<T, Allocator>> : std::true_type {};
    // 3. 辅助变量模板 (C++17 风格)
    template <typename T>
    inline constexpr bool is_vector_v = is_vector<T>::value;
    template <typename T>
    inline T get_column_value(sqlite3_stmt* stmt, const int col)
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            const unsigned char* text = sqlite3_column_text(stmt, col);
            const auto bytes = sqlite3_column_bytes(stmt, col);
            return text ? std::string(reinterpret_cast<const char*>(text), bytes) : "";
        } else if constexpr (std::is_integral_v<T>) {
            if constexpr (sizeof(T) <= 4) {
                return static_cast<T>(sqlite3_column_int(stmt, col));
            } else if constexpr (sizeof(T) == 8){
                return static_cast<T>(sqlite3_column_int64(stmt, col));
            }
            else
            {
                static_assert(0, "unsupported size of type for get_column_value");
            }
        } else if constexpr (std::is_floating_point_v<T>) {
            return static_cast<T>(sqlite3_column_double(stmt, col));
        } else if constexpr (is_vector_v<T>)
        {
            using ElementType = typename T::value_type;

            // 确保只处理像 vector<uint8_t> 或 vector<char> 这样的字节容器
            static_assert(sizeof(ElementType) == 1, "Only 1-byte element vectors are supported for BLOB");

            const void* blob_ptr = sqlite3_column_blob(stmt, col);
            int bytes = sqlite3_column_bytes(stmt, col);

            if (blob_ptr && bytes > 0) {
                const ElementType* start = static_cast<const ElementType*>(blob_ptr);
                return T(start, start + bytes);
            }
            return T{};
        } else
        {
            static_assert(0, "unsupported type for get_column_value");
        }
        return T{};
    }

    inline void bind_parameter_null(sqlite3_stmt* stmt, int index)
    {
        sqlite3_bind_null(stmt, index);
    }
    template<typename T>
    std::enable_if_t<std::is_integral_v<T> && (sizeof(T) <=4)> bind_parameter(sqlite3_stmt* stmt, int index, const T& value)
    {
        sqlite3_bind_int(stmt, index, static_cast<int>(value));
    }
    template<typename T>
    std::enable_if_t<std::is_integral_v<T> && (sizeof(T) ==8)> bind_parameter(sqlite3_stmt* stmt, int index, const T& value)
    {
        sqlite3_bind_int64(stmt, index, static_cast<sqlite3_int64>(value));
    }
    template<typename T>
    std::enable_if_t<std::is_integral_v<T> && sizeof(T)==1> bind_parameter(sqlite3_stmt* stmt, int index, const std::vector<T>& value)
    {
        sqlite3_bind_blob(stmt, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
    }
    // template<typename T>
    // void bind_parameter(sqlite3_stmt* stmt, int index, const T& value);
    inline void bind_parameter(sqlite3_stmt* stmt, int index, const double& value)
    {
        sqlite3_bind_double(stmt, index, value);
    }
    inline void bind_parameter(sqlite3_stmt* stmt, int index, const float& value)
    {
        sqlite3_bind_double(stmt, index, static_cast<double>(value));
    }
    inline void bind_parameter(sqlite3_stmt* stmt, int index, const std::string& value)
    {
        sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
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
            ResultSet()= default;
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
            // : Database("test.db", strategy) { }
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
        T query_one(std::unique_ptr<sqlite3_stmt, StatementDeleter> stmt) const
        {
            if (!is_open())
                throw std::runtime_error("database is not open");
            // 此处无需再次调用sqlite3_prepare_v2，因为stmt已经是准备好的语句
            auto rs = ResultSet<T>(std::move(stmt));
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

}
#endif // BACKUPSUITE_DATABASE_H
