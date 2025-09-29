//
// Created by ycm on 2025/9/24.
//

#include <gtest/gtest.h>

#include "core/core_utils.h"
#include "utils/database.h"
#include "utils/tar.h"

TEST(TestSqlite3, TestOpenDatabase)
{
    GTEST_LOG_(INFO) << "SQLite3 Version: " << sqlite3_libversion() << "\n";
    sqlite3* db = nullptr;
    int rc = sqlite3_open(":memory:", &db);
    ASSERT_EQ(rc, SQLITE_OK);
    ASSERT_NE(db, nullptr);
    sqlite3_close(db);
}

TEST(TestTar, TestOpenTar)
{
    const std::filesystem::path raw_tar_path = R"(C:\Users\ycm\Desktop\New Folder\tar\white.tar)";
    tar::TarFile tar(raw_tar_path);
    auto results = tar.list_dir(".");
    GTEST_LOG_(INFO) << "Tar list_dir: " << results.size() << "\n";
    for (auto &[meta, offset] : results)
    {
        print_meta(meta, GTEST_LOG_(INFO) << "Offset: " << offset << "\n");
    }
}
