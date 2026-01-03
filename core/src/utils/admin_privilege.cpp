//
// Created by ycm on 2025/9/8.
//
#include "utils/admin_privilege.h"

static bool checked_admin = false;
static bool is_admin = false;

bool BACKUP_SUITE_API is_running_as_admin()
{
    if (checked_admin) return is_admin;
    checked_admin = true;
#ifdef _WIN32
    BOOL isAdmin = FALSE;
    HANDLE hToken = nullptr;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        BYTE adminSID[SECURITY_MAX_SID_SIZE];
        DWORD dwSize = sizeof(adminSID);
        if (CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, &adminSID, &dwSize)) {
            CheckTokenMembership(hToken, adminSID, &isAdmin);
        }
        CloseHandle(hToken);
    }
    is_admin = isAdmin;
#elif defined(__linux__)
    is_admin = geteuid() == 0;
    return geteuid() == 0;
#elif defined(__APPLE__)
#endif
    return is_admin;
}
