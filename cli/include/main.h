//
// Created by ycm on 25-9-3.
//

#ifndef MAIN_H
#define MAIN_H

#include "system_device.h"

struct BackupConfig
{

};

class BackupSuite
{
    BackupConfig config = {};
public:
    BackupSuite() = default;
    // ReSharper disable once CppNonExplicitConvertingConstructor
    BackupSuite(const BackupConfig& cfg): config(cfg) {} // NOLINT(*-explicit-constructor)
    void run_backup(const device &from, device &to) const;
};

#endif //MAIN_H
