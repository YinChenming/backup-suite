//
// Created by ycm on 2025/9/8.
//

#ifndef BACKUPSUITE_BACKUP_CONTROLLER_H
#define BACKUPSUITE_BACKUP_CONTROLLER_H
#pragma once

#include <queue>

#include "api.h"
#include "filesystem/device.h"

struct BackupConfig
{
    bool backup_symbolic_links = false;
    FileEntityType backup_file_types = FileEntityType::RegularFile | FileEntityType::Directory;
};

class BACKUP_SUITE_API BackupController
{
    BackupConfig config = {};
public:
    BackupController() = default;
    // ReSharper disable once CppNonExplicitConvertingConstructor
    BackupController(const BackupConfig& cfg): config(cfg) {} // NOLINT(*-explicit-constructor)
    void run_backup(Device &from, Device &to) const;
};

#endif // BACKUPSUITE_BACKUP_CONTROLLER_H
