//
// Created by ycm on 2025/9/24.
//

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <archive.h>
#include <archive_entry.h>

#include "core/core_utils.h"
#include "utils/database.h"
#include "utils/tar.h"
#include "utils/zip.h"
#include "filesystem/entities.h"

#ifndef ssize_t

#ifdef _MSC_VER
#include <BaseTsd.h>
using ssize_t = SSIZE_T;
#elif defined(_WIN64)
using ssize_t = __int64;
#else
using ssize_t = long;
#endif

#endif

TEST(TestSqlite3, TestOpenDatabase)
{
    GTEST_LOG_(INFO) << "SQLite3 Version: " << sqlite3_libversion() << "\n";
    sqlite3* db = nullptr;
    int rc = sqlite3_open(":memory:", &db);
    ASSERT_EQ(rc, SQLITE_OK);
    ASSERT_NE(db, nullptr);
    sqlite3_close(db);
}

// 简单的测试文件实现，用于测试压缩功能
class TestFile : public ReadableFile {
public:
    explicit TestFile(const std::string& path, const std::string& content)
        : ReadableFile(create_meta(path, content)), m_position(0)
    {
        // Convert std::string to std::vector<std::byte>
        m_content.reserve(content.size());
        for (char c : content) {
            m_content.push_back(static_cast<std::byte>(c));
        }
    }

    ~TestFile() override = default;

    [[nodiscard]] std::unique_ptr<std::vector<std::byte>> read() override {
        auto result = std::make_unique<std::vector<std::byte>>(m_content);
        m_position = m_content.size();
        return result;
    }

    [[nodiscard]] std::unique_ptr<std::vector<std::byte>> read(size_t size) override {
        size_t bytes_to_read = std::min(size, m_content.size() - m_position);
        auto result = std::make_unique<std::vector<std::byte>>(
            m_content.begin() + m_position,
            m_content.begin() + m_position + bytes_to_read
        );
        m_position += bytes_to_read;
        return result;
    }
private:
    std::vector<std::byte> m_content;
    size_t m_position;

    static FileEntityMeta create_meta(const std::string& path, const std::string& content) {
        FileEntityMeta meta;
        meta.path = path;
        meta.type = FileEntityType::RegularFile;
        meta.size = content.size();
        meta.access_time = std::chrono::system_clock::now();
        meta.modification_time = std::chrono::system_clock::now();
        meta.creation_time = std::chrono::system_clock::now();
        return meta;
    }
};

TEST(TestTar, TestTarCreate) {
    // 创建一个临时文件用于测试
    const std::string test_content = "Hello, this is a test file for tar compression!";
    TestFile test_file("test.txt", test_content);

    // 创建tar文件
    const std::string tar_path = "test_output.tar";
    tar::TarFile tar(tar_path, tar::TarFile::output);

    // 添加文件到tar
    bool success = tar.add_entity(test_file);
    ASSERT_TRUE(success);

    // 完成tar创建
    tar.close();

    // 验证tar文件存在
    ASSERT_TRUE(std::filesystem::exists(tar_path));

    // 清理
    std::filesystem::remove(tar_path);

    GTEST_LOG_(INFO) << "Tar create test passed!" << std::endl;
}
TEST(TestZip, TestZipCreate) {
    // 创建一个临时文件用于测试
    const std::string test_content = "Hello, this is a test file for zip compression!";
    TestFile test_file("test.txt", test_content);

    // 创建zip文件
    const std::string zip_path = "test_output.zip";
    zip::ZipFile zip(zip_path, zip::ZipFile::ZipMode::output);

    // 添加文件到zip
    bool success = zip.add_entity(test_file);
    ASSERT_TRUE(success);

    // 完成zip创建
    zip.close();

    // 验证zip文件存在
    ASSERT_TRUE(std::filesystem::exists(zip_path));

    // 清理
    std::filesystem::remove(zip_path);

    GTEST_LOG_(INFO) << "Zip create test passed!" << std::endl;
}

TEST(TestZip, TestZipRead) {
    // 创建一个临时文件用于测试
    const std::string test_content = "Hello, this is a test file for zip compression!";
    const std::string test_filename = "test.txt";
    TestFile test_file(test_filename, test_content);

    // 创建zip文件
    const std::string zip_path = "test_read.zip";
    {
        zip::ZipFile zip(zip_path, zip::ZipFile::ZipMode::output);
        bool success = zip.add_entity(test_file);
        ASSERT_TRUE(success);
        zip.close();
    }

    // 重新打开zip文件进行读取
    {
        zip::ZipFile zip_reader(zip_path, zip::ZipFile::ZipMode::input);

        // 测试list_dir
        auto results = zip_reader.list_dir(".");
        ASSERT_EQ(results.size(), 1);

        auto& cdfh = results[0];
        const auto offset = cdfh.record.local_header_offset;
        auto meta = zip::ZipFile::cdfh_to_file_meta(cdfh);
        ASSERT_EQ(meta.path, test_filename);
        ASSERT_EQ(meta.type, FileEntityType::RegularFile);
        ASSERT_EQ(meta.size, test_content.size());

        // 测试get_file_stream
        auto stream = zip_reader.get_file_stream(test_filename);
        ASSERT_NE(stream, nullptr);
        print_meta(stream->get_meta(), GTEST_LOG_(INFO) << "Zip read file meta:\n");

        // 读取文件内容
        std::string read_content;
        char buffer[1024];
        while (true) {
            stream->read(buffer, sizeof(buffer));
            size_t bytes_read = stream->gcount();
            if (bytes_read == 0) {
                break;
            }
            read_content.append(buffer, bytes_read);
        }

        ASSERT_EQ(read_content, test_content);
        zip_reader.close();
    }

    // 清理
    std::filesystem::remove(zip_path);

    GTEST_LOG_(INFO) << "Zip read test passed!" << std::endl;
}
// 使用libarchive读取tar文件的辅助函数
bool libarchive_read_tar(const std::string& tar_path, const std::string& expected_file, const std::string& expected_content) {
    struct archive *a;
    struct archive_entry *entry;
    int r;

    a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    r = archive_read_open_filename(a, tar_path.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        archive_read_free(a);
        return false;
    }

    bool found = false;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char* pathname = archive_entry_pathname(entry);
        if (pathname == expected_file) {
            size_t size = archive_entry_size(entry);
            std::vector<char> buffer(size);
            ssize_t bytes_read = archive_read_data(a, buffer.data(), size);
            if (bytes_read == size) {
                std::string content(buffer.data(), size);
                if (content == expected_content) {
                    found = true;
                    break;
                }
            }
        }
        archive_read_data_skip(a);
    }

    r = archive_read_free(a);
    return found && (r == ARCHIVE_OK);
}

// 使用libarchive创建tar文件的辅助函数
bool libarchive_create_tar(const std::string& tar_path, const std::string& file_path, const std::string& content) {
    struct archive *a;
    struct archive_entry *entry;
    const void *buff = content.data();
    size_t size = content.size();
    la_int64_t offset = 0;

    a = archive_write_new();
    // WARNING! DO NOT add gzip filter here, it will make the tar file unreadable by our custom tar reader.
    // archive_write_add_filter_gzip(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, tar_path.c_str());

    entry = archive_entry_new();
    archive_entry_set_pathname(entry, file_path.c_str());
    archive_entry_set_size(entry, size);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    archive_write_data(a, buff, size);
    archive_entry_free(entry);

    archive_write_close(a);
    archive_write_free(a);
    return true;
}

TEST_F(TestSystemDevice, TestCrossCompatibility) {
    // 测试1：自定义实现创建tar，libarchive读取
    const std::string test_content = "This is a test for cross compatibility!";
    const std::string test_filepath = "it_is_a_file_with_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_long_of_cross_test.txt";
    TestFile test_file(test_filepath, test_content);

    // 创建tar文件
    const std::string tar_path = "cross_test.tar";
    tar::TarFile tar_writer(tar_path, tar::TarFile::output);
    ASSERT_TRUE(tar_writer.add_entity(test_file));
    tar_writer.close();

    tar::TarFile tar_writer_reader(tar_path, tar::TarFile::input);
    ASSERT_TRUE(tar_writer_reader.get_standard() == tar::TarStandard::POSIX_2001_PAX || tar_writer_reader.get_standard() == tar::TarStandard::POSIX_1988_USTAR);
    auto stream = tar_writer_reader.get_file_stream(test_filepath);
    ASSERT_NE(stream, nullptr);
    tar_writer_reader.close();

    // 使用libarchive读取刚创建的tar文件
    bool libarchive_result = libarchive_read_tar(tar_path, test_filepath, test_content);
    ASSERT_TRUE(libarchive_result);
    GTEST_LOG_(INFO) << "Custom tar -> libarchive read test passed!" << std::endl;

    // 清理
    std::filesystem::remove(tar_path);

    // 测试2：libarchive创建tar，自定义实现读取
    const std::string libarchive_tar_path = "libarchive_test.tar";
    const std::string libarchive_file_path = "it_is_a_file_with_a_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_long_filename_of_libarchive_file.txt";
    const std::string libarchive_content = "This is a test file created by libarchive!";

    // 使用libarchive创建tar文件
    ASSERT_TRUE(libarchive_create_tar(libarchive_tar_path, libarchive_file_path, libarchive_content));

    // 使用自定义实现读取tar文件
    tar::TarFile tar_reader(libarchive_tar_path, tar::TarFile::input);
    ASSERT_TRUE(tar_reader.get_standard() == tar::TarStandard::POSIX_2001_PAX || tar_reader.get_standard() == tar::TarStandard::POSIX_1988_USTAR);
    auto results = tar_reader.list_dir(".");

    // 验证内容
    bool found = false;
    for (auto& [meta, offset] : results) {
        if (meta.path == libarchive_file_path) {
            print_meta(meta, GTEST_LOG_(INFO) << "test tar-libarchive cross compatibility: find file\n");
            auto file_stream = tar_reader.get_file_stream(libarchive_file_path);
            ASSERT_NE(file_stream, nullptr);

            std::string content;
            char buffer[1024];
            while (true) {
                file_stream->read(buffer, sizeof(buffer));
                size_t bytes_read = file_stream->gcount();
                if (bytes_read == 0) {
                    break;
                }
                content.append(buffer, bytes_read);
            }

            ASSERT_EQ(content, libarchive_content);
            found = true;
            GTEST_LOG_(INFO) << "Libarchive tar -> custom read test passed!" << std::endl;
            break;
        }
    }
    tar_reader.close();

    ASSERT_TRUE(found);

    // 清理
    std::filesystem::remove(libarchive_tar_path);

    GTEST_LOG_(INFO) << "All cross compatibility tests passed!" << std::endl;
}
