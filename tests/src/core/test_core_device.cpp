//
// Created by ycm on 25-9-5.
//
#include <fstream>
#include <bitset>
#include <gtest/gtest.h>

#include "device.h"
#include "system_device.h"

std::string get_file_type_name(const FileEntityType type)
{
    switch (type)
    {
        case FileEntityType::Directory:
            return "Directory";
        case FileEntityType::RegularFile:
            return "RegularFile";
        case FileEntityType::SymbolicLink:
            return "SymbolicLink";
        case FileEntityType::Fifo:
            return "Fifo";
        case FileEntityType::Socket:
            return "Socket";
        case FileEntityType::BlockDevice:
            return "BlockDevice";
        case FileEntityType::CharacterDevice:
            return "CharacterDevice";
        default:
            return "Unknown";
    }
}

void print_meta(const FileEntityMeta& meta, std::ostream& os = std::cout)
{
    const auto ctime = std::chrono::system_clock::to_time_t(meta.creation_time);
    const auto mtime = std::chrono::system_clock::to_time_t(meta.modification_time);
    const auto atime = std::chrono::system_clock::to_time_t(meta.access_time);

    std::tm ctm{}, mtm{}, atm{};
#ifdef _MSC_VER
    localtime_s(&ctm, &ctime);
    localtime_s(&mtm, &mtime);
    localtime_s(&atm, &atime);
#else
    ctm = *std::localtime(&ctime);
    mtm = *std::localtime(&mtime);
    atm = *std::localtime(&atime);
#endif

    os << "Path: " << meta.path << "\n"
    << "Type: " << get_file_type_name(meta.type) << "\n"
    << "Size: " << meta.size << "\n"
    << "Modification Time: " << std::put_time(&mtm, "%Y-%m-%d %H:%M:%S") << "\n"
    << "Access Time: " << std::put_time(&atm, "%Y-%m-%d %H:%M:%S") << "\n"
    << "---------- For Windows ----------\n"
    << "Creation Time: " << std::put_time(&ctm, "%Y-%m-%d %H:%M:%S") << "\n"
    << "Windows Attributes: " << std::bitset<sizeof(meta.windows_attributes) * 8>(meta.windows_attributes) << "\n"
    << "---------- For POSIX ----------\n"
    << "POSIX Mode: " << std::oct << meta.posix_mode << std::dec << "\n"
    << "User Name: " << meta.user_name << "\n"
    << "Group Name: " << meta.group_name << "\n"
    << "User ID: " << meta.uid << "\n"
    << "Group ID: " << meta.gid << "\n"
    << "Symbolic Link Target: " << meta.symbolic_link_target << "\n"
    << "Device Major: " << meta.device_major << "\n"
    << "Device Minor: " << meta.device_minor << "\n"
    ;
}

void print_file(File& file, std::ostream& os = std::cout)
{
    os << "File Metadata:\n";
    print_meta(file.get_meta(), os);
}

void print_folder(Folder& folder, std::ostream& os = std::cout)
{
    os << "Folder Metadata:\n";
    print_meta(folder.get_meta(), os);
    os << "Children:\n";
    for (auto& child : folder.get_children())
    {
        os << " - " << child.get_meta().path.filename() << " (" << get_file_type_name(child.get_meta().type) << ")\n";
    }
}

TEST(TestSystemDevice, BasicAssertions) {
#ifdef _WIN32
    const auto current_path = std::filesystem::current_path() / "test";
    const std::filesystem::path test_folder_path = "test_folder";
    GTEST_LOG_(INFO) << "Current path: " << current_path << "\n";
    if (std::filesystem::exists(current_path))
    {
        std::filesystem::remove_all(current_path);
    }
    std::filesystem::create_directory(current_path);
    auto device = WindowsDevice(current_path );
    std::filesystem::create_directory(current_path / test_folder_path);

    auto folder = device.get_folder(test_folder_path);
    ASSERT_NE(folder, nullptr);
    EXPECT_EQ(folder->get_children().size(), 0);
    print_folder(*folder, GTEST_LOG_(INFO) << "Test Empty Folder via get_folder:\n");


    // 测试普通文件
    if (std::ofstream ofs(current_path / test_folder_path / "test_file.txt", std::ios::out | std::ios::trunc); ofs.is_open())
    {
        ofs << "Hello, World!" << std::endl;
        ofs.close();

        GTEST_LOG_(INFO) << "create file test_file.txt";
    } else
    {
        GTEST_LOG_(ERROR) << "Failed to open test_file.txt";
    }

    auto file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file(test_folder_path / "test_file.txt").release());
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(std::string(std::istreambuf_iterator(file->get_stream()), {}), "Hello, World!\n");
    file->close();

    auto meta = std::make_unique<FileEntityMeta>(file->get_meta());
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, FileEntityType::RegularFile);
    print_meta(*meta, GTEST_LOG_(INFO) << "Test Normal File:\n");


    // 测试隐藏文件
    if (std::ofstream ofs(current_path / test_folder_path / "test_file_hide.txt", std::ios::out | std::ios::trunc); ofs.is_open())
    {
        ofs << "Hello, World!(hide)" << std::endl;
        ofs.close();

        GTEST_LOG_(INFO) << "create file test_file_hide.txt";
    } else
    {
        GTEST_LOG_(ERROR) << "Failed to open test_file_hide.txt";
    }
    SetFileAttributesW((current_path / test_folder_path / "test_file_hide.txt").wstring().c_str(), FILE_ATTRIBUTE_HIDDEN);

    file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file(test_folder_path / "test_file_hide.txt").release());
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(std::string(std::istreambuf_iterator(file->get_stream()), {}), "Hello, World!(hide)\n");
    file->close();

    meta = std::make_unique<FileEntityMeta>(file->get_meta());
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, FileEntityType::RegularFile);
    print_meta(*meta, GTEST_LOG_(INFO) << "Test Hide File:\n");


    // 测试只读文件
    if (std::ofstream ofs(current_path / test_folder_path / "test_file_readonly.txt", std::ios::out | std::ios::trunc); ofs.is_open())
    {
        ofs << "Hello, World!(readonly)" << std::endl;
        ofs.close();

        GTEST_LOG_(INFO) << "create file test_file_readonly.txt";
    } else
    {
        GTEST_LOG_(ERROR) << "Failed to open test_file_readonly.txt";
    }
    SetFileAttributesW((current_path / test_folder_path / "test_file_readonly.txt").wstring().c_str(), FILE_ATTRIBUTE_READONLY);

    file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file(test_folder_path / "test_file_readonly.txt").release());
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(std::string(std::istreambuf_iterator(file->get_stream()), {}), "Hello, World!(readonly)\n");
    file->close();
    meta = std::make_unique<FileEntityMeta>(file->get_meta());
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, FileEntityType::RegularFile);
    print_meta(*meta, GTEST_LOG_(INFO) << "Test Readonly File:\n");


    // 测试嵌套文件夹
    std::filesystem::create_directory(current_path / test_folder_path / test_folder_path);
    folder = device.get_folder(test_folder_path);
    ASSERT_NE(folder, nullptr);
    EXPECT_EQ(folder->get_children().size(), 4);
    print_folder(*folder, GTEST_LOG_(INFO) << "Test Folder via get_folder:\n");

    // test symbolic link
    if (std::filesystem::exists(current_path / "test_link_folder"))
    {
        meta = device.get_meta("test_link_folder");
        ASSERT_NE(meta, nullptr);
        EXPECT_EQ(meta->type, FileEntityType::SymbolicLink);
        print_meta(*meta, GTEST_LOG_(INFO) << "Test Symbolic Link Folder:\n");
    }

    if (std::filesystem::exists(current_path / "test_link_file"))
    {
        meta = device.get_meta("test_link_file");
        ASSERT_NE(meta, nullptr);
        EXPECT_EQ(meta->type, FileEntityType::SymbolicLink);
        print_meta(*meta, GTEST_LOG_(INFO) << "Test Symbolic Link File:\n");
    }

    try
    {
        std::filesystem::remove_all(current_path);
    } catch (...)
    {
        GTEST_LOG_(ERROR) << "Failed to remove test directory: " << "\n";
    }
#elif defined __linux__
#endif
}
