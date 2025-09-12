//
// Created by ycm on 25-9-5.
//
#include <fstream>
#include <bitset>
#include <gtest/gtest.h>

#ifdef _WIN32
#include <windows.h>
#elif defined __linux__
#include <sys/stat.h>
#endif

#include "filesystem/device.h"
#include "filesystem/system_device.h"

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

bool run_script_as_admin(const std::filesystem::path& script_path)
{
#ifdef _WIN32
    const std::wstring interpreter = L"C:\\Windows\\System32\\cmd.exe";
    const std::wstring params = L"/C \"" + script_path.wstring() + L"\"";

    SHELLEXECUTEINFOW sei = { sizeof(sei) };

    // SEE_MASK_NOCLOSEPROGRESS 使 ShellExecuteExW 调用后会返回新进程的句柄供后续使用
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = nullptr;
    sei.lpVerb = L"runas"; // 以管理员权限运行
    sei.lpFile = interpreter.c_str(); // 解释器路径
    sei.lpParameters = params.c_str(); // 脚本路径
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei))
    {
        const DWORD error = GetLastError();
        if (error == ERROR_CANCELLED)
        {
            GTEST_LOG_(INFO) << "User cancelled the UAC prompt.";
            return false;
        }
        throw std::runtime_error("Failed to execute script as admin. Error code: " + std::to_string(error));
    }

    WaitForSingleObject(sei.hProcess, INFINITE);
    CloseHandle(sei.hProcess);
    return true;
#elif defined __linux__
    throw std::runtime_error("Cannot run script as admin on Linux");
#endif
}

class TestSystemDevice: public ::testing::Test
{
public:
    inline static const std::filesystem::path root = std::filesystem::current_path() / "test";
    inline static const std::filesystem::path test_folder = "test_folder";
    inline static const auto device = SystemDevice(root);
    inline static bool volatile test_symbolic_link =
#ifdef _WIN32
        false;
#elif __linux__
            true;
#endif

protected:
    // static void SetUpTestCase()
    // {
    //     SetUpSuitCase();
    // }
    static void SetUpTestSuite()
    {
        SetUpSuitCase();
    }
    static void SetUpSuitCase()
    {
        if (std::filesystem::exists(root))
        {
            std::filesystem::remove_all(root);
        }
        std::filesystem::create_directory(root);
        std::filesystem::create_directory(root / test_folder);

        // 测试普通文件
        if (std::ofstream ofs(root / test_folder / "test_file.txt", std::ios::out | std::ios::trunc); ofs.is_open())
        {
            ofs << "Hello, World!" << std::endl;
            ofs.close();

            GTEST_LOG_(INFO) << "create file test_file.txt";
        } else
        {
            GTEST_LOG_(ERROR) << "Failed to open test_file.txt";
        }

        // 测试隐藏文件
        if (std::ofstream ofs(root / test_folder / "test_file_hide.txt", std::ios::out | std::ios::trunc); ofs.is_open())
        {
            ofs << "Hello, World!(hide)" << std::endl;
            ofs.close();

            GTEST_LOG_(INFO) << "create file test_file_hide.txt";
#ifdef _WIN32
            SetFileAttributesW((root / test_folder / "test_file_hide.txt").wstring().c_str(), FILE_ATTRIBUTE_HIDDEN);
#endif
        } else
        {
            GTEST_LOG_(ERROR) << "Failed to open test_file_hide.txt";
        }

        // 测试只读文件
        if (std::ofstream ofs(root / test_folder / "test_file_readonly.txt", std::ios::out | std::ios::trunc); ofs.is_open())
        {
            ofs << "Hello, World!(readonly)" << std::endl;
            ofs.close();

            GTEST_LOG_(INFO) << "create file test_file_readonly.txt";
#ifdef _WIN32
            SetFileAttributesW((root / test_folder / "test_file_readonly.txt").wstring().c_str(), FILE_ATTRIBUTE_READONLY);
#elif defined __linux__
            chmod((root / test_folder / "test_file_readonly.txt").c_str(), 0444);
#endif
        } else
        {
            GTEST_LOG_(ERROR) << "Failed to open test_file_readonly.txt";
        }

        // 测试嵌套文件夹
        std::filesystem::create_directory(root / test_folder / test_folder);


        // 测试符号链接
#ifdef _WIN32
        if (std::ofstream ofs(root / "create_sl.bat", std::ios::out | std::ios::trunc); ofs.is_open())
        {
            ofs << "@echo off" << std::endl;
            ofs << "cd \"" << root.string() << "\"" << std::endl;
            ofs << "mklink /D \"" << (root / "test_link_folder").string() << "\" \"" << (root / test_folder).string() << "\"" << std::endl;
            ofs << "mklink \"" << (root / "test_link_file").string() << "\" \"" << (root / test_folder / "test_file.txt").string() << "\"" << std::endl;
            ofs.close();
            test_symbolic_link = true;
        } else
        {
            test_symbolic_link = false;
            GTEST_LOG_(ERROR) << "Failed to open create_sl.bat";
        }
        if (test_symbolic_link)
        {
            try
            {
                run_script_as_admin(root / "create_sl.bat");
                GTEST_LOG_(INFO) << "create symbolic link";
            } catch (const std::exception& e)
            {
                GTEST_LOG_(ERROR) << "Failed to create symbolic link: " << e.what() << "\n";
                test_symbolic_link = false;
            }
            std::filesystem::remove(root / "create_sl.bat");
        }
#elif defined __linux__
#endif
    }

    // static void TearDownTestCase()
    // {
    //     TearDownSuitCase();
    // }
    static void TearDownTestSuite()
    {
        TearDownSuitCase();
    }
    static void TearDownSuitCase()
    {
        if (std::filesystem::exists(root / test_folder / "test_file_readonly.txt"))
        {
#ifdef _WIN32
            SetFileAttributesW((root / test_folder / "test_file_readonly.txt").wstring().c_str(), FILE_ATTRIBUTE_NORMAL);
#elif defined __linux__
            chmod((root / test_folder / "test_file_readonly.txt").c_str(), 0777);
#endif
        }
        try
        {
            std::filesystem::remove_all(root);
        } catch (...)
        {
            GTEST_LOG_(ERROR) << "Failed to remove test directory: " << "\n";
        }
    }
};

TEST_F(TestSystemDevice, TestFolder)
{
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
    EXPECT_EQ(std::string(std::istreambuf_iterator(file->get_stream()), {}), "Hello, World!\n");
    file->close();
    auto meta = std::make_unique<FileEntityMeta>(file->get_meta());
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, FileEntityType::RegularFile);
    print_meta(*meta, GTEST_LOG_(INFO) << "Test Normal File:\n");

    // test hide file
    file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file(test_folder / "test_file_hide.txt").release());
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(std::string(std::istreambuf_iterator(file->get_stream()), {}), "Hello, World!(hide)\n");
    file->close();
    meta = std::make_unique<FileEntityMeta>(file->get_meta());
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, FileEntityType::RegularFile);
#ifdef _WIN32
    EXPECT_TRUE(meta->windows_attributes & FILE_ATTRIBUTE_HIDDEN);
#endif
    print_meta(*meta, GTEST_LOG_(INFO) << "Test Hide File"
#ifdef __linux__
    << "(Linux does not support hidden attribute)"
#endif
    << ":\n");

    // test readonly file
    file = dynamic_cast<PhysicalDeviceReadableFile*>(device.get_file(test_folder / "test_file_readonly.txt").release());
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(std::string(std::istreambuf_iterator(file->get_stream()), {}), "Hello, World!(readonly)\n");
    file->close();
    meta = std::make_unique<FileEntityMeta>(file->get_meta());
    ASSERT_NE(meta, nullptr);
    EXPECT_EQ(meta->type, FileEntityType::RegularFile);
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

    meta.release();
}
