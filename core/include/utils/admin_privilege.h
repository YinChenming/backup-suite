//
// Created by ycm on 2025/9/8.
//

#ifndef BACKUPSUITE_ADMIN_PRIVILEGES_H
#define BACKUPSUITE_ADMIN_PRIVILEGES_H

#include "api.h"

#ifdef _WIN32
#include <windows.h>
#elif defined __linux__
#include <unistd.h>
#else
#error "Unsupported platform"
static_assert(false, "Unsupported platform");
#endif

bool BACKUP_SUITE_API is_running_as_admin();

#endif // BACKUPSUITE_ADMIN_PRIVILEGES_H
