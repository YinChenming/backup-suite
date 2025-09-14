//
// Created by ycm on 2025/9/15.
//

#include <gtest/gtest.h>

#include "backup/backup_controller.h"
#include "core/core_utils.h"

TEST_F(TestSystemDevice, TestBackUp)
{
    const auto backup_path = root / "backup";
    if (std::filesystem::exists(backup_path))
        std::filesystem::remove_all(backup_path);
    std::filesystem::create_directory(backup_path);
    SystemDevice backup_device(backup_path);
    BackupController controller{};
    controller.run_backup(device, backup_device);
    print_folder(*backup_device.get_folder(test_folder), GTEST_LOG_(INFO) << "Backup Root Folder:\n");
}
