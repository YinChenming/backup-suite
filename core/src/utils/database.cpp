//
// Created by ycm on 2025/9/24.
//

#include "utils/database.h"

using namespace db;

[[nodiscard]] bool Database::is_initialized() const
{
    if (!is_open())
        return false;
    sqlite3_stmt* stmt = nullptr;
    if (const auto sql = "SELECT name FROM sqlite_master WHERE type='table';";
        sqlite3_prepare_v2(db_handle_.get(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    const bool initialized = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return initialized;
}

void Database::initialize(const DatabaseInitializationStrategy &strategy) const
{
    if (!is_open() || strategy.get_initialization_sql().empty())
        return;
    char* errMsg = nullptr;
    sqlite3_exec(db_handle_.get(), strategy.get_initialization_sql().c_str(), nullptr, nullptr, &errMsg);
    if (errMsg)
        sqlite3_free(errMsg);
}

[[nodiscard]] bool Database::exec(const std::string& sql, const bool commit) const
{
    if (!is_open())
        return false;
    char* errMsg = nullptr;
    if (const int rc = sqlite3_exec(db_handle_.get(), sql.c_str(), nullptr, nullptr, &errMsg); rc != SQLITE_OK)
    {
        if (errMsg)
            sqlite3_free(errMsg);
        return false;
    }
    if (commit)
        sqlite3_exec(db_handle_.get(), "COMMIT;", nullptr, nullptr, &errMsg);
    return true;
}

[[nodiscard]] bool Database::execute(const sqlite3_stmt& stmt, const bool commit) const
{
    if (!is_open())
        return false;
    if (const int rc = sqlite3_step(const_cast<sqlite3_stmt*>(&stmt)); rc != SQLITE_DONE && rc != SQLITE_ROW)
        return false;
    if (commit)
        sqlite3_exec(db_handle_.get(), "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}
