//
// Created by ycm on 2025/9/8.
//
#include "utils/admin_privilege.h"

bool BACKUP_SUITE_API is_running_as_admin()
{
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
    return isAdmin;
#elif defined __linux__
    return geteuid() == 0;
#endif
}
