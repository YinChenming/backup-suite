//
// Created by ycm on 25-9-5.
//
#include <fstream>
#include <bitset>
#include <string>
#include <gtest/gtest.h>

#ifdef _WIN32
#include <windows.h>

#define NEWLINE (std::string("\r\n"))
#undef CHECK_ACCESS_TIME
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/stat.h>

#define NEWLINE (std::string("\n"))
#define CHECK_ACCESS_TIME
#else
#error "Unsupported platform"
static_assert(false, "Unsupported platform");
#endif

#include "filesystem/device.h"
#include "filesystem/system_device.h"

#include "core/core_utils.h"

TEST_F(TestSystemDevice, TestFolder)
{
    print_folder(*device.get_folder("."), GTEST_LOG_(INFO) << "Root Folder:\n");

    // test empty folder
    auto folder = device.get_folder(test_folder / test_folder);
    ASSERT_NE(folder, nullptr);
    EXPECT_EQ(folder->get_children().size(), 0);
    auto meta = std::make_unique<FileEntityMeta>(folder->get_meta());
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, FileEntityType::Directory);
    print_folder(*folder, GTEST_LOG_(INFO) << "Test Empty Folder:\n");

    // test root folder
    folder = device.get_folder(test_folder);
    ASSERT_NE(folder, nullptr);
    EXPECT_EQ(folder->get_children().size(), 4);
    meta = std::make_unique<FileEntityMeta>(folder->get_meta());
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, FileEntityType::Directory);
    print_folder(*folder, GTEST_LOG_(INFO) << "Test Root Folder:\n");
}


TEST_F(TestSystemDevice, TestFiles)
{
    // test normal file
    auto file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file(test_folder / "test_file.txt").release());
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(std::string(std::istreambuf_iterator(file->get_stream()), {}), test_file_content);
    file->close();
    auto meta = std::make_unique<FileEntityMeta>(file->get_meta());
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, FileEntityType::RegularFile);
    EXPECT_EQ(meta->size, test_file_content.size());
    print_meta(*meta, GTEST_LOG_(INFO) << "Test Normal File:\n");

    // test hide file
    file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file(test_folder / "test_file_hide.txt").release());
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(std::string(std::istreambuf_iterator(file->get_stream()), {}), test_file_hide_content);
    file->close();
    meta = std::make_unique<FileEntityMeta>(file->get_meta());
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, FileEntityType::RegularFile);
    EXPECT_EQ(meta->size, test_file_hide_content.size());
#ifdef _WIN32
    EXPECT_TRUE(meta->windows_attributes & FILE_ATTRIBUTE_HIDDEN);
#endif
    print_meta(*meta, GTEST_LOG_(INFO) << "Test Hide File"
#ifdef __APPLE__
    << "(MacOS does not support hidden attribute)"
#elif defined(__linux__)
    << "(Linux does not support hidden attribute)"
#endif
    << ":\n");

    // test readonly file
    file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file(test_folder / "test_file_readonly.txt").release());
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(std::string(std::istreambuf_iterator(file->get_stream()), {}), test_file_readonly_content);
    file->close();
    meta = std::make_unique<FileEntityMeta>(file->get_meta());
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, FileEntityType::RegularFile);
    EXPECT_EQ(meta->size, test_file_readonly_content.size());
#ifdef _WIN32
    EXPECT_TRUE(meta->windows_attributes & FILE_ATTRIBUTE_READONLY);
#endif
    EXPECT_EQ(meta->posix_mode, 0444);
    print_meta(*meta, GTEST_LOG_(INFO) << "Test Readonly File:\n");
}

TEST_F(TestSystemDevice, TestSymbolicLink)
{
    if (!test_symbolic_link)
    {
        GTEST_LOG_(WARNING) << "Test Symbolic Link does not exist.\n";
        return;
    }

    auto meta = device.get_meta("test_link_folder");
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, FileEntityType::SymbolicLink);
    print_meta(*meta, GTEST_LOG_(INFO) << "Test Symbolic Link Folder:\n");


    meta = device.get_meta("test_link_file");
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, FileEntityType::SymbolicLink);
    print_meta(*meta, GTEST_LOG_(INFO) << "Test Symbolic Link File:\n");
}

TEST_F(TestSystemDevice, TestWriteFolder)
{
    auto folder = device.get_folder(test_folder);
    ASSERT_NE(folder, nullptr);
    FileEntityMeta meta = folder->get_meta(), new_meta;
    meta.path = test_write_folder;
    Folder tmp_folder(meta, {});
    ASSERT_TRUE(device.write_folder(tmp_folder));

    folder = device.get_folder(test_write_folder);
    ASSERT_NE(folder, nullptr);
    auto new_folder = device.get_folder(test_write_folder);
    ASSERT_NE(new_folder, nullptr);

    meta = folder->get_meta();
    new_meta = new_folder->get_meta();


#ifdef CHECK_ACCESS_TIME
    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::microseconds>(new_meta.access_time.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(meta.access_time.time_since_epoch()).count()
    );
#endif

    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::microseconds>(new_meta.creation_time.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(meta.creation_time.time_since_epoch()).count()
    );
    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::microseconds>(new_meta.modification_time.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(meta.modification_time.time_since_epoch()).count()
    );
    EXPECT_EQ(new_meta.posix_mode, meta.posix_mode);
    EXPECT_EQ(new_meta.windows_attributes, meta.windows_attributes);
    EXPECT_EQ(new_meta.type, meta.type);

    print_folder(*new_folder, GTEST_LOG_(INFO) << "Test Write Folder:\n");
}

TEST_F(TestSystemDevice, TestWriteFile)
{
    auto file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file(test_folder / "test_file.txt").release());
    ASSERT_NE(file, nullptr);
    FileEntityMeta meta, new_meta;
    meta = file->get_meta();
    file->get_meta().path = "test_write_file.txt";
    ASSERT_TRUE(device.write_file(*file));
    file->close();
    auto new_file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file("test_write_file.txt").release());
    ASSERT_NE(new_file, nullptr);
    EXPECT_EQ(std::string(std::istreambuf_iterator(new_file->get_stream()), {}), test_file_content);
    new_file->close();
    new_meta = new_file->get_meta();


#ifdef CHECK_ACCESS_TIME
    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::microseconds>(new_meta.access_time.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(meta.access_time.time_since_epoch()).count()
    );
#endif

    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::microseconds>(new_meta.creation_time.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(meta.creation_time.time_since_epoch()).count()
    );
    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::microseconds>(new_meta.modification_time.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(meta.modification_time.time_since_epoch()).count()
    );
    EXPECT_EQ(new_meta.posix_mode, meta.posix_mode);
    EXPECT_EQ(new_meta.windows_attributes, meta.windows_attributes);
    EXPECT_EQ(new_meta.size, meta.size);
    EXPECT_EQ(new_meta.type, meta.type);
    print_file(*new_file, GTEST_LOG_(INFO) << "Test Write File:\n");

    file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file(test_folder / "test_file_readonly.txt").release());
    ASSERT_NE(file, nullptr);
    meta = file->get_meta();
    file->get_meta().path = "test_write_file_readonly.txt";
    ASSERT_TRUE(device.write_file(*file));
    file->close();
    new_file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file("test_write_file_readonly.txt").release());
    ASSERT_NE(new_file, nullptr);
    EXPECT_EQ(std::string(std::istreambuf_iterator(new_file->get_stream()), {}), test_file_readonly_content);
    new_file->close();
    new_meta = new_file->get_meta();


#ifdef CHECK_ACCESS_TIME
    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::microseconds>(new_meta.access_time.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(meta.access_time.time_since_epoch()).count()
    );
#endif

    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::microseconds>(new_meta.creation_time.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(meta.creation_time.time_since_epoch()).count()
    );
    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::microseconds>(new_meta.modification_time.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(meta.modification_time.time_since_epoch()).count()
    );
    EXPECT_EQ(new_meta.posix_mode, meta.posix_mode);
    EXPECT_EQ(new_meta.windows_attributes, meta.windows_attributes);
    EXPECT_EQ(new_meta.size, meta.size);
    EXPECT_EQ(new_meta.type, meta.type);
    print_file(*new_file, GTEST_LOG_(INFO) << "Test Write Readonly File:\n");

    file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file(test_folder / "test_file_hide.txt").release());
    ASSERT_NE(file, nullptr);
    meta = file->get_meta();
    file->get_meta().path = "test_write_file_hide.txt";
    ASSERT_TRUE(device.write_file(*file));
    file->close();
    new_file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file("test_write_file_hide.txt").release());
    ASSERT_NE(new_file, nullptr);
    EXPECT_EQ(std::string(std::istreambuf_iterator(new_file->get_stream()), {}), test_file_hide_content);
    new_file->close();
    new_meta = new_file->get_meta();


#ifdef CHECK_ACCESS_TIME
    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::microseconds>(new_meta.access_time.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(meta.access_time.time_since_epoch()).count()
    );
#endif

    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::microseconds>(new_meta.creation_time.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(meta.creation_time.time_since_epoch()).count()
    );
    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::microseconds>(new_meta.modification_time.time_since_epoch()).count(),
        std::chrono::duration_cast<std::chrono::microseconds>(meta.modification_time.time_since_epoch()).count()
    );
    EXPECT_EQ(new_meta.posix_mode, meta.posix_mode);
    EXPECT_EQ(new_meta.windows_attributes, meta.windows_attributes);
    EXPECT_EQ(new_meta.size, meta.size);
    EXPECT_EQ(new_meta.type, meta.type);
    print_file(*new_file, GTEST_LOG_(INFO) << "Test Write Hide File:\n");
}
