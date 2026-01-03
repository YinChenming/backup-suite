//
// Created by ycm on 2025/12/27.
//

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

#include "core/core_utils.h"
#include "backup/backup_controller.h"
#include "filesystem/compresses_device.h"
#include "filesystem/entities.h"
#include "filesystem/system_device.h"
#include "utils/tmpfile.h"

namespace fs = std::filesystem;
using namespace tmpfile_utils;

TEST_F(TestSystemDevice, TestTarDevice)
{
    const auto tmp_tar_file = TmpFile::create();
    GTEST_LOG_(INFO) << "TestTarDevice tmp tar path: " << tmp_tar_file->path() << "\n";
    // 首先创建一个tar备份文件
    {
        TarDevice tar_device(tmp_tar_file->path(), TarDevice::Mode::WriteOnly);
        BackupController controller{};
        controller.run_backup(device, tar_device);
    }

    // 现在测试只读TarDevice
    {
        TarDevice read_tar_device(tmp_tar_file->path(), TarDevice::Mode::ReadOnly);
        EXPECT_TRUE(read_tar_device.is_open());

        // 测试获取文件 - 使用正确的路径
        auto test_file = read_tar_device.get_file(test_folder / "test_file.txt");
        EXPECT_NE(test_file, nullptr);

        // 读取文件内容
        auto content = test_file->read();
        ASSERT_NE(content, nullptr);
        std::string file_content(reinterpret_cast<char*>(content->data()), content->size());
        GTEST_LOG_(INFO) << "Read file 'test_folder/test_file.txt' content: " << file_content << "\n";
        EXPECT_EQ(file_content, test_file_content);

        // 测试获取目录
        auto folder = read_tar_device.get_folder(test_folder);
        EXPECT_NE(folder, nullptr);

        // 测试文件存在性
        EXPECT_TRUE(read_tar_device.exists(test_folder / "test_file.txt"));
        EXPECT_TRUE(read_tar_device.exists(test_folder));
        EXPECT_FALSE(read_tar_device.exists("nonexistent_file.txt"));
    }
}
TEST_F(TestSystemDevice, TestZipDevice)
{
    const auto tmp_zip_file = TmpFile::create();
    GTEST_LOG_(INFO) << "TestZipDevice tmp zip path: " << tmp_zip_file->path() << "\n";
    // 首先创建一个zip备份文件
    {
        ZipDevice zip_device(tmp_zip_file->path(), ZipDevice::Mode::WriteOnly);
        BackupController controller{};
        controller.run_backup(device, zip_device);
    }

    // 现在测试只读ZipDevice
    {
        ZipDevice read_zip_device(tmp_zip_file->path(), ZipDevice::Mode::ReadOnly);
        EXPECT_TRUE(read_zip_device.is_open());

        // 测试获取文件 - 使用正确的路径
        auto test_file = read_zip_device.get_file(test_folder / "test_file.txt");
        EXPECT_NE(test_file, nullptr);

        // 读取文件内容
        auto content = test_file->read();
        ASSERT_NE(content, nullptr);
        std::string file_content(reinterpret_cast<char*>(content->data()), content->size());
        GTEST_LOG_(INFO) << "Read file 'test_folder/test_file.txt' content: " << file_content << "\n";
        EXPECT_EQ(file_content, test_file_content);

        // 测试获取目录
        auto folder = read_zip_device.get_folder(test_folder);
        EXPECT_NE(folder, nullptr);

        // 测试文件存在性
        EXPECT_TRUE(read_zip_device.exists(test_folder / "test_file.txt"));
        EXPECT_TRUE(read_zip_device.exists(test_folder));
        EXPECT_FALSE(read_zip_device.exists("nonexistent_file.txt"));
    }
}
TEST_F(TestSystemDevice, TestZipEncDevice)
{
    const auto tmp_zip_file = TmpFile::create();
    GTEST_LOG_(INFO) << "TestZipDevice tmp zip path: " << tmp_zip_file->path() << "\n";
    std::vector<uint8_t> password = {'p', 'a', 's', 's', 'w', 'o', 'r', 'd'};
    for (const zip::header::ZipEncryptionMethod enc_method : {zip::header::ZipEncryptionMethod::ZipCrypto, zip::header::ZipEncryptionMethod::RC4})
    {
        std::string enc_method_name;
        switch (enc_method)
        {
            case zip::header::ZipEncryptionMethod::ZipCrypto:
                enc_method_name = "ZipCrypto";
                break;
            case zip::header::ZipEncryptionMethod::RC4:
                enc_method_name = "RC4";
                break;
            default: enc_method_name = "Unknown"; break;
        }
        GTEST_LOG_(INFO) << "Testing Zip Encryption Method: " << enc_method_name << "\n";
        // 首先创建一个zip备份文件
        {
            ZipDevice zip_device(tmp_zip_file->path(), ZipDevice::Mode::WriteOnly, password);
            zip_device.set_encryption_method(enc_method);
            BackupController controller{};
            controller.run_backup(device, zip_device);
        }

        // 现在测试只读ZipDevice
        {
            ZipDevice read_zip_device(tmp_zip_file->path(), ZipDevice::Mode::ReadOnly, password);
            read_zip_device.set_encryption_method(enc_method);
            EXPECT_TRUE(read_zip_device.is_open());

            // 测试获取文件 - 使用正确的路径
            auto test_file = read_zip_device.get_file(test_folder / "test_file.txt");
            EXPECT_NE(test_file, nullptr);

            // 读取文件内容
            auto content = test_file->read();
            ASSERT_NE(content, nullptr);
            std::string file_content(reinterpret_cast<char*>(content->data()), content->size());
            GTEST_LOG_(INFO) << "Read file 'test_folder/test_file.txt' content: " << file_content << "\n";
            EXPECT_EQ(file_content, test_file_content);

            // 测试获取目录
            auto folder = read_zip_device.get_folder(test_folder);
            EXPECT_NE(folder, nullptr);

            // 测试文件存在性
            EXPECT_TRUE(read_zip_device.exists(test_folder / "test_file.txt"));
            EXPECT_TRUE(read_zip_device.exists(test_folder));
            EXPECT_FALSE(read_zip_device.exists("nonexistent_file.txt"));
        }
    }
}
TEST_F(TestSystemDevice, TestBackupFromPhysicalToTar)
{
    const auto tmp_tar_file = TmpFile::create();
    GTEST_LOG_(INFO) << "TestBackupFromPhysicalToTar tmp tar path: " << tmp_tar_file->path() << "\n";
    // 模拟从物理设备备份到tar设备
    {
        TarDevice tar_device(tmp_tar_file->path(), TarDevice::Mode::WriteOnly);
        BackupController controller{};
        controller.run_backup(device, tar_device);
    }

    // 验证备份
    {
        TarDevice verify_tar(tmp_tar_file->path(), TarDevice::Mode::ReadOnly);

        // 检查文件是否存在
        EXPECT_TRUE(verify_tar.exists(test_folder / "test_file.txt"));
        EXPECT_TRUE(verify_tar.exists(test_folder));

        // 验证文件内容
        auto file = verify_tar.get_file(test_folder / "test_file.txt");
        ASSERT_NE(file, nullptr);
        auto content = file->read();
        ASSERT_NE(content, nullptr);
        std::string file_content(reinterpret_cast<char*>(content->data()), content->size());
        GTEST_LOG_(INFO) << "Read backed up file 'test_folder/test_file.txt' content: " << file_content << "\n";
        EXPECT_EQ(file_content, test_file_content);
    }
}
TEST_F(TestSystemDevice, TestBackupFromPhysicalToZip)
{
    const auto tmp_zip_file = TmpFile::create();
    GTEST_LOG_(INFO) << "TestBackupFromPhysicalToZip tmp zip path: " << tmp_zip_file->path() << "\n";
    // 模拟从物理设备备份到zip设备
    {
        ZipDevice zip_device(tmp_zip_file->path(), ZipDevice::Mode::WriteOnly);
        BackupController controller{};
        controller.run_backup(device, zip_device);
    }

    // 验证备份
    {
        ZipDevice verify_zip(tmp_zip_file->path(), ZipDevice::Mode::ReadOnly);

        // 检查文件是否存在
        EXPECT_TRUE(verify_zip.exists(test_folder / "test_file.txt"));
        EXPECT_TRUE(verify_zip.exists(test_folder));

        // 验证文件内容
        auto file = verify_zip.get_file(test_folder / "test_file.txt");
        ASSERT_NE(file, nullptr);
        auto content = file->read();
        ASSERT_NE(content, nullptr);
        std::string file_content(reinterpret_cast<char*>(content->data()), content->size());
        GTEST_LOG_(INFO) << "Read backed up file 'test_folder/test_file.txt' content: " << file_content << "\n";
        EXPECT_EQ(file_content, test_file_content);
    }
}
TEST_F(TestSystemDevice, TestInvalidDevice)
{
    // 测试不存在的文件
    {
        TarDevice invalid_tar("nonexistent_file.tar", TarDevice::Mode::ReadOnly);
        EXPECT_FALSE(invalid_tar.is_open());
        EXPECT_EQ(invalid_tar.get_file("any_file.txt"), nullptr);
        EXPECT_EQ(invalid_tar.get_folder("any_folder"), nullptr);
        EXPECT_FALSE(invalid_tar.exists("any_file"));
    }

    // 测试zip设备
    {
        ZipDevice invalid_zip("nonexistent_file.zip", ZipDevice::Mode::ReadOnly);
        EXPECT_FALSE(invalid_zip.is_open());
        EXPECT_EQ(invalid_zip.get_file("any_file.txt"), nullptr);
        EXPECT_EQ(invalid_zip.get_folder("any_folder"), nullptr);
        EXPECT_FALSE(invalid_zip.exists("any_file"));
    }
}
TEST_F(TestSystemDevice, TestEmptyPath)
{
    // 测试空路径
    {
        TarDevice tar_device("test_empty_path.tar", TarDevice::Mode::ReadOnly);
        EXPECT_EQ(tar_device.get_file(""), nullptr);
        EXPECT_EQ(tar_device.get_folder(""), nullptr);
        EXPECT_FALSE(tar_device.exists(""));
    }

    {
        ZipDevice zip_device("test_empty_path.tar", ZipDevice::Mode::ReadOnly);
        EXPECT_EQ(zip_device.get_file(""), nullptr);
        EXPECT_EQ(zip_device.get_folder(""), nullptr);
        EXPECT_FALSE(zip_device.exists(""));
    }
}
TEST_F(TestSystemDevice, TestBackupControllerPhysicalToTar)
{
    const auto tmp_tar_file = TmpFile::create();
    GTEST_LOG_(INFO) << "TestBackupControllerPhysicalToTar tmp tar path: " << tmp_tar_file->path() << "\n";
    // 使用BackupController从物理设备备份到TarDevice
    {
        TarDevice tar_device(tmp_tar_file->path(), TarDevice::Mode::WriteOnly);
        BackupController controller{};
        controller.run_backup(device, tar_device);
    }

    // 验证备份结果
    {
        TarDevice read_tar_device(tmp_tar_file->path(), TarDevice::Mode::ReadOnly);
        EXPECT_TRUE(read_tar_device.is_open());

        // 检查文件是否存在
        EXPECT_TRUE(read_tar_device.exists(test_folder / "test_file.txt"));
        EXPECT_TRUE(read_tar_device.exists(test_folder));
    }
}
TEST_F(TestSystemDevice, TestBackupControllerPhysicalToZip)
{
    const auto tmp_zip_file = TmpFile::create();
    GTEST_LOG_(INFO) << "TestBackupControllerPhysicalToZip tmp zip path: " << tmp_zip_file->path() << "\n";
    // 使用BackupController从物理设备备份到ZipDevice
    {
        ZipDevice zip_device(tmp_zip_file->path(), ZipDevice::Mode::WriteOnly);
        BackupController controller{};
        controller.run_backup(device, zip_device);
        zip_device.close();
    }

    // 验证备份结果
    {
        ZipDevice read_zip_device(tmp_zip_file->path(), ZipDevice::Mode::ReadOnly);
        EXPECT_TRUE(read_zip_device.is_open());

        // 检查文件是否存在
        EXPECT_TRUE(read_zip_device.exists(test_folder / "test_file.txt"));
        EXPECT_TRUE(read_zip_device.exists(test_folder));
    }
}
TEST_F(TestSystemDevice, TestBackupControllerTarToPhysical)
{
    const auto tmp_tar_file = TmpFile::create();
    GTEST_LOG_(INFO) << "TestBackupControllerTarToPhysical tmp tar path: " << tmp_tar_file->path() << "\n";
    // 首先创建tar备份
    {
        TarDevice tar_device(tmp_tar_file->path(), TarDevice::Mode::WriteOnly);
        BackupController controller{};
        controller.run_backup(device, tar_device);
        tar_device.close();
    }

    // 使用BackupController从TarDevice备份到物理设备
    {
        const std::filesystem::path restore_dir = root / "restore_tar";
        if (std::filesystem::exists(restore_dir))
            std::filesystem::remove_all(restore_dir);
        std::filesystem::create_directories(restore_dir);

        TarDevice source_tar_device(tmp_tar_file->path(), TarDevice::Mode::ReadOnly);
        std::unique_ptr<Folder> root_folder = source_tar_device.get_folder({});
        print_folder(*root_folder, GTEST_LOG_(INFO) << "Source Tar Root Folder Meta:\n");
        SystemDevice restore_device(restore_dir);
        BackupController controller{};
        controller.run_backup(source_tar_device, restore_device);

        // 验证恢复结果
        SystemDevice verify_restore_device(restore_dir);
        EXPECT_TRUE(verify_restore_device.exists(test_folder));
        EXPECT_TRUE(verify_restore_device.exists(test_folder / "test_file.txt"));
    }
}
TEST_F(TestSystemDevice, TestBackupControllerZipToPhysical)
{
    const auto tmp_zip_file = TmpFile::create();
    GTEST_LOG_(INFO) << "TestBackupControllerZipToPhysical tmp zip path: " << tmp_zip_file->path() << "\n";
    // 首先创建zip备份
    {
        ZipDevice zip_device(tmp_zip_file->path(), ZipDevice::Mode::WriteOnly);
        BackupController controller{};
        controller.run_backup(device, zip_device);
        zip_device.close();
    }

    // 使用BackupController从ZipDevice备份到物理设备
    {
        const std::filesystem::path restore_dir = root / "restore_zip";
        if (std::filesystem::exists(restore_dir))
            std::filesystem::remove_all(restore_dir);
        std::filesystem::create_directories(restore_dir);

        ZipDevice source_zip_device(tmp_zip_file->path(), ZipDevice::Mode::ReadOnly);
        std::unique_ptr<Folder> root_folder = (source_zip_device.get_folder({}));
        print_folder(*root_folder, GTEST_LOG_(INFO) << "Source Zip Root Folder Meta:\n");
        SystemDevice restore_device(restore_dir);
        BackupController controller{};
        controller.run_backup(source_zip_device, restore_device);

        // 验证恢复结果
        SystemDevice verify_restore_device(restore_dir);
        EXPECT_TRUE(verify_restore_device.exists(test_folder));
        EXPECT_TRUE(verify_restore_device.exists(test_folder / "test_file.txt"));
    }
}