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
#elif defined __linux__
#include <sys/stat.h>

#define NEWLINE (std::string("\n"))
#define CHECK_ACCESS_TIME
#else
#error "Unsupported platform"
static_assert(false, "Unsupported platform");
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
    inline static const std::filesystem::path test_write_folder = "test_write_folder";

    inline static const std::string test_file_content = "Hello, World!" + NEWLINE;
    inline static const std::string test_file_hide_content = "Hello, World!(hide)" + NEWLINE;
    inline static const std::string test_file_readonly_content = "Hello, World!(readonly)" + NEWLINE;

    inline static auto device = SystemDevice(root);
    inline static bool volatile test_symbolic_link =
#ifdef _WIN32
        false;
#elif __linux__
            true;
#endif

protected:
#ifdef GTEST_NEW_SETUP_NAME
    static void SetUpTestSuite()
#else
    static void SetUpTestCase()
#endif
    // static void SetUpSuitCase()
    {
        if (std::filesystem::exists(root))
        {
            std::filesystem::remove_all(root);
        }
        std::filesystem::create_directory(root);
        std::filesystem::create_directory(root / test_folder);

        // 测试普通文件
        if (std::ofstream ofs(root / test_folder / "test_file.txt", std::ios::out | std::ios::trunc | std::ios::binary); ofs.is_open())
        {
            ofs << test_file_content;
            ofs.flush();
            ofs.close();

            GTEST_LOG_(INFO) << "create file test_file.txt";
        } else
        {
            GTEST_LOG_(ERROR) << "Failed to open test_file.txt";
        }

        // 测试隐藏文件
        if (std::ofstream ofs(root / test_folder / "test_file_hide.txt", std::ios::out | std::ios::trunc | std::ios::binary); ofs.is_open())
        {
            ofs << test_file_hide_content;
            ofs.flush();
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
        if (std::ofstream ofs(root / test_folder / "test_file_readonly.txt", std::ios::out | std::ios::trunc | std::ios::binary); ofs.is_open())
        {
            ofs << test_file_readonly_content;
            ofs.flush();
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
                test_symbolic_link = run_script_as_admin(root / "create_sl.bat");
                if (test_symbolic_link)
                    GTEST_LOG_(INFO) << "create symbolic link";
                else
                    GTEST_LOG_(ERROR) << "User cancelled the UAC prompt, symbolic link not created.\n"
                << "To enable symbolic link tests, please run the test suite again and accept the UAC prompt.";
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

#ifdef GTEST_NEW_SETUP_NAME
    static void TearDownTestSuite()
#else
    static void TearDownTestCase()
#endif
    // static void TearDownSuitCase()
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
#ifdef __linux__
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
