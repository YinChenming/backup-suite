//
// Created by ycm on 2025/9/8.
//

#ifndef BACKUPSUITE_BACKUP_CONTROLLER_H
#define BACKUPSUITE_BACKUP_CONTROLLER_H

#include "api.h"
#include "filesystem/device.h"

struct BackupConfig
{
    bool backup_symbolic_links = false;
};

class BACKUP_SUITE_API BackupSuite
{
    BackupConfig config = {};
public:
    BackupSuite() = default;
    // ReSharper disable once CppNonExplicitConvertingConstructor
    BackupSuite(const BackupConfig& cfg): config(cfg) {} // NOLINT(*-explicit-constructor)
    void run_backup(const Device &from, Device &to) const;
};

#endif // BACKUPSUITE_BACKUP_CONTROLLER_H
