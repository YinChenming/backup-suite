//
// Created by ycm on 25-9-3.
//

#include <fstream>

#include "system_device.h"

using namespace utils::time_converter;

#ifdef _WIN32

std::unique_ptr<Folder> WindowsDevice::get_folder(const std::filesystem::path& path) const
{
    // throw std::runtime_error("Not implemented");
    const std::unique_ptr<FileEntityMeta> meta = get_meta(path);
    if (!meta || meta->type != FileEntityType::Directory)
    {
        return nullptr;
    }
    std::vector<FileEntity> children;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW ffd;
    hFind = FindFirstFileW((root / path / "*").c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        GetLastError();
        return std::make_unique<Folder>(*std::move(meta), std::move(children));
    }
    while (FindNextFileW(hFind, &ffd))
    {
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0)
        {
            continue;
        }
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            children.push_back(*get_folder(path / ffd.cFileName));
        } else
        {
            children.push_back(*get_file(path / ffd.cFileName));
        }
    } ;

    FindClose(hFind);
    return std::make_unique<Folder>(*std::move(meta), std::move(children));
}

std::unique_ptr<ReadableFile> WindowsDevice::get_file(const std::filesystem::path& path) const
{
    const std::unique_ptr<FileEntityMeta> meta = get_meta(path);
    if (!meta || meta->type != FileEntityType::RegularFile)
    {
        return nullptr;
    }
    std::unique_ptr<std::ifstream> fs(new std::ifstream(root / path));
    if (!fs || !fs->good())
    {
        return nullptr;
    }
    return std::make_unique<PhysicalDeviceReadableFile>(*std::move(meta), std::move(fs));
}


std::unique_ptr<std::ifstream> WindowsDevice::get_file_stream(const std::filesystem::path& path) const
{
    return std::make_unique<std::ifstream>(root / path);
}

/**
 *
 * @param path
 * @return FileEntityMeta对象，如果路径无效或不是文件则返回空对象
 */
std::unique_ptr<FileEntityMeta> WindowsDevice::get_meta(const std::filesystem::path& path) const
{
    const auto realpath = root / path;
    // get file attributes
    const DWORD attributes = GetFileAttributesW(realpath.wstring().c_str());
    // return if error or is directory
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return nullptr;
    }

    FileEntityMeta meta;

    HANDLE hFile = CreateFileW(
        realpath.wstring().c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS | // FILE_FLAG_BACKUP_SEMANTICS 允许打开目录
        FILE_FLAG_OPEN_REPARSE_POINT,  // 允许打开符号链接本身而不是目标
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
        return nullptr;
    }

    // get file type
    switch (GetFileType(hFile))
    {
        case FILE_TYPE_DISK: meta.type = attributes & FILE_ATTRIBUTE_DIRECTORY ? FileEntityType::Directory : FileEntityType::RegularFile; break;
        case FILE_TYPE_PIPE: meta.type = FileEntityType::Fifo; break;
        case FILE_TYPE_CHAR: meta.type = FileEntityType::CharacterDevice; break;
        default: meta.type = FileEntityType::Unknown; break;
    }

    // deal with symbolic link
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT)
    {
        do
        {
            // 获取符号链接的目标路径
            HANDLE hFileLink = INVALID_HANDLE_VALUE;
            WIN32_FIND_DATAW ffd;
            hFileLink = FindFirstFileW(realpath.wstring().c_str(), &ffd);
            FindClose(hFileLink);
            // 根据FindFirstFile的dwReserved0最终确定是否为符号链接
            if (hFileLink != INVALID_HANDLE_VALUE && ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT && ffd.dwReserved0 & IO_REPARSE_TAG_SYMLINK)
            {
                meta.type = FileEntityType::SymbolicLink;
            } else break;

            // 打开符号链接目标文件
            hFileLink = CreateFileW(
                realpath.wstring().c_str(),GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, // FILE_FLAG_BACKUP_SEMANTICS 允许打开目录
                // 打开符号链接目标
                nullptr
            );
            if (hFileLink == INVALID_HANDLE_VALUE)
            {
                CloseHandle(hFileLink);
                break;
            }

            WCHAR linkPath[MAX_PATH], tmpPath[MAX_PATH], devicePath[MAX_PATH];

            // GetFinalPathNameByHandle获取软链接对应的真实设备路径,设置VOLUME_NAME_NT获取最好的兼容性
            // VOLUME_NAME_DOS 以\\?\C:\格式的DOS路径开头
            // VOLUME_NAME_GUID 以\\?\Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\的卷 GUID 路径开头
            // VOLUME_NAME_NT 以\device\HardDiskVolumeX\格式的设备名称开头
            // 更多用法参见 https://learn.microsoft.com/zh-cn/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew
            DWORD dwRet = GetFinalPathNameByHandleW(hFileLink, linkPath, MAX_PATH, FILE_NAME_NORMALIZED | VOLUME_NAME_NT);
            std::wstring linkPathW, tmpPathW, targetPathW;
            // dwRet=0 表示发生错误, dwRet>=MAX_PATH 表示缓冲区不足
            if (!dwRet || dwRet >= MAX_PATH)
            {
                CloseHandle(hFileLink);
                break;
            }
            linkPathW = std::wstring(linkPath, dwRet);

            // GetVolumePathName 根据路径获取盘符
            // 例如 \device\HardDiskVolume3\Windows\System32\notepad.exe -> "C:\"
            if (!GetVolumePathNameW(linkPath, tmpPath, MAX_PATH))
            {
                CloseHandle(hFileLink);
                break;
            }
            tmpPath[2] = L'\0'; // 截断为 "C:", QueryDosDevice不能查询以'\'结尾的路径
            tmpPathW = std::wstring(tmpPath);
            // QueryDosDevice 将盘符转换为设备路径
            // 例如 "C:" -> "\device\HardDiskVolume3"
            if (!QueryDosDeviceW(tmpPath, devicePath, MAX_PATH))
            {
                CloseHandle(hFileLink);
                break;
            }
            auto devicePathLen = std::wcslen(devicePath);   // 设备路径长度
            // 拼接出符号链接的目标路径
            // 例如 "C:" + "\device\HardDiskVolume3\Windows\System32\notepad.exe".substr(len("\device\HardDiskVolume3")) -> "C:\Windows\System32\notepad.exe"
            meta.symbolic_link_target = tmpPathW + linkPathW.substr(devicePathLen, linkPathW.length() - devicePathLen);

            CloseHandle(hFileLink);
        } while (false);

        // 获取符号链接的目标路径
        // HANDLE hFileLink = INVALID_HANDLE_VALUE;
        // WIN32_FIND_DATAW ffd;
        // hFileLink = FindFirstFileW(realpath.wstring().c_str(), &ffd);
        // if (hFileLink != INVALID_HANDLE_VALUE && ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT && ffd.dwReserved0 & IO_REPARSE_TAG_SYMLINK)
        // {
        //     meta.type = FileEntityType::SymbolicLink;
        //     FindClose(hFileLink);
        //     hFileLink = CreateFileW(
        //         realpath.wstring().c_str(),GENERIC_READ,
        //         FILE_SHARE_READ,
        //         nullptr,
        //         OPEN_EXISTING,
        //         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, // FILE_FLAG_BACKUP_SEMANTICS 允许打开目录
        //         // 打开符号链接目标
        //         nullptr
        //     );
        //
        //     // GetFinalPathNameByHandle获取软链接对应的真实设备路径
        //     // GetVolumePathName获取卷路径
        //     WCHAR linkPath[MAX_PATH], tmpPath[MAX_PATH], devicePath[MAX_PATH];
        //     DWORD dwRet = GetFinalPathNameByHandleW(hFileLink, linkPath, MAX_PATH, FILE_NAME_NORMALIZED | VOLUME_NAME_NT);
        //     std::wstring linkPathW, tmpPathW, targetPathW;
        //     if (dwRet < MAX_PATH)
        //     {
        //         linkPathW = std::wstring(linkPath, dwRet);
        //         // 获取盘符
        //         if (GetVolumePathNameW(linkPath, tmpPath, MAX_PATH))
        //         {
        //             tmpPath[2] = L'\0'; // 截断为 "C:", QueryDosDevice不能以'\'结尾
        //             tmpPathW = std::wstring(tmpPath);
        //             if (QueryDosDeviceW(tmpPath, devicePath, MAX_PATH))
        //             {
        //                 auto devicePathLen = std::wcslen(devicePath);
        //                 meta.symbolic_link_target = tmpPathW + linkPathW.substr(devicePathLen, linkPathW.length() - devicePathLen);
        //             }
        //         }
        //         CloseHandle(hFileLink);
        //     }
        // }else
        // {
        //     FindClose(hFileLink);
        //     meta.symbolic_link_target = L"";
        // }
    }

    // set file mode
    // Windows上的符号链接文件有独立的权限,不会同步目标文件的权限
    if (attributes & FILE_ATTRIBUTE_READONLY)
    {
        meta.posix_mode = 0444; // 只读权限
    } else
    {
        meta.posix_mode = 0777; // 读写权限
    }
    meta.windows_attributes = attributes;

    meta.path = std::filesystem::absolute(realpath);
    LARGE_INTEGER file_size;
    // 使用Windows API 获取文件大小
    if (!GetFileSizeEx(hFile, &file_size))
    {
        CloseHandle(hFile);
        return nullptr;
    }
    meta.size = file_size.QuadPart;

    // get file time
    FILETIME ftCreate, ftAccess, ftWrite;
    if (!GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite))
    {
        CloseHandle(hFile);
        return nullptr;
    }

    auto ft2chrono = [](const FILETIME &ft) -> std::chrono::system_clock::time_point {
        ULARGE_INTEGER ull;
        ull.LowPart = ft.dwLowDateTime;
        ull.HighPart = ft.dwHighDateTime;
        return std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    std::chrono::nanoseconds((ull.QuadPart - TICKS_FROM_FILESYSTEM_TO_UTC_EPOCH) * 100)
                )
            );
    };
    meta.creation_time = ft2chrono(ftCreate);
    meta.modification_time = ft2chrono(ftWrite);
    meta.access_time = ft2chrono(ftAccess);

    CloseHandle(hFile);

    // --- 核心：获取 user_name 和 group_name ---
    PSID owner_sid = nullptr;
    PSID group_sid = nullptr;
    PSECURITY_DESCRIPTOR sec_desc = nullptr;

    // 获取文件的所有者和组 SID
    DWORD result = GetNamedSecurityInfoW(
        realpath.c_str(),
        SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
        &owner_sid,
        &group_sid,
        nullptr, // DACL
        nullptr, // SACL
        &sec_desc
    );
    if (result == ERROR_SUCCESS)
    {
        auto owner_name = sid2name(owner_sid);
        auto group_name = sid2name(group_sid);

        // 将 wstring 转换为 string(utf-8)
        int owner_name_size = WideCharToMultiByte(CP_UTF8, 0, owner_name.c_str(), -1, nullptr, 0, nullptr, nullptr);
        int group_name_size = WideCharToMultiByte(CP_UTF8, 0, group_name.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (owner_name_size > 0)
        {
            std::string owner_name_utf8(owner_name_size, 0);
            WideCharToMultiByte(CP_UTF8, 0, owner_name.c_str(), -1, &owner_name_utf8[0], owner_name_size, nullptr, nullptr);
            meta.user_name = owner_name_utf8.substr(0, owner_name_utf8.size() - 1);
        } else
        {
            meta.user_name = "";
        }
        if (group_name_size > 0)
        {
            std::string group_name_utf8(group_name_size, 0);
            WideCharToMultiByte(CP_UTF8, 0, group_name.c_str(), -1, &group_name_utf8[0], group_name_size, nullptr, nullptr);
            meta.group_name = group_name_utf8.substr(0, group_name_utf8.size() - 1);
        } else
        {
            meta.group_name = "";
        }
    } else
    {
        meta.user_name = meta.group_name = "";
    }
    // 释放由 GetNameSecurityInfoW 分配的内存
    if (sec_desc != nullptr)
    {
        LocalFree(sec_desc);
    }

    return std::make_unique<FileEntityMeta>(meta);
}

bool WindowsDevice::exists(const std::filesystem::path& path) const
{
    const DWORD attributes = GetFileAttributesW((root / path).wstring().c_str());
    // return if error
    return attributes != INVALID_FILE_ATTRIBUTES;
}

bool WindowsDevice::write_file(const ReadableFile &file)
{
    return false;
}

bool WindowsDevice::write_folder(const Folder &folder)
{
    return false;
}

#elif defined __linux__
#endif
