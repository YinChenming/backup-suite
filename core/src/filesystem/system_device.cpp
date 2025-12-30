//
// Created by ycm on 25-9-3.
//

#include <fstream>

#include "filesystem/system_device.h"

using namespace utils::time_converter;

#ifdef _WIN32

std::unique_ptr<Folder> WindowsDevice::get_folder(const std::filesystem::path& path, bool recursion)
{
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
            if (recursion)
            {
                if (const auto child_folder = get_folder(path / ffd.cFileName); child_folder != nullptr)
                    children.emplace_back(*child_folder);
            }
            else
            {
                if (const auto file_meta = get_meta(path / ffd.cFileName); file_meta != nullptr)
                    children.emplace_back(Folder{*file_meta, {}});
            }
        } else
        {
            if (const auto file_meta = get_meta(path / ffd.cFileName))
                children.emplace_back(File{*file_meta});
        }
    }

    FindClose(hFind);
    return std::make_unique<Folder>(*std::move(meta), std::move(children));
}

std::unique_ptr<ReadableFile> WindowsDevice::get_file(const std::filesystem::path& path)
{
    const std::unique_ptr<FileEntityMeta> meta = get_meta(path);
    if (!meta || meta->type != FileEntityType::RegularFile)
    {
        return nullptr;
    }
    std::unique_ptr<std::ifstream> fs(new std::ifstream(root / path, std::ios::in | std::ios::binary));
    if (!fs || !fs->good())
    {
        return nullptr;
    }
    return std::make_unique<PhysicalDeviceReadableFile>(*std::move(meta), std::move(fs));
}


std::unique_ptr<std::ifstream> WindowsDevice::get_file_stream(const std::filesystem::path& path) const
{
    return std::filesystem::exists(root / path) && std::filesystem::is_regular_file(root / path) ?
        std::make_unique<std::ifstream>(root / path) :
        nullptr;
}

/**
 *
 * @param path
 * @return FileEntityMeta对象，如果路径无效或不是文件则返回空对象
 */
std::unique_ptr<FileEntityMeta> WindowsDevice::get_meta(const std::filesystem::path& path)
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

    meta.path = std::filesystem::relative(realpath, root);
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

bool WindowsDevice::exists(const std::filesystem::path& path)
{
    const DWORD attributes = GetFileAttributesW((root / path).wstring().c_str());
    // return if error
    return attributes != INVALID_FILE_ATTRIBUTES;
}

bool WindowsDevice::write_file(ReadableFile& file) {
    return _write_file(file, false);
}

bool WindowsDevice::write_file_force(ReadableFile &file)
{
    return _write_file(file, true);
}

bool WindowsDevice::_write_file(ReadableFile &file, const bool force)
{
    const auto meta = file.get_meta();
    const auto realpath = root / meta.path;
    if (exists(realpath) && !force) {
        return false;
    }
    if (meta.type == FileEntityType::SymbolicLink)
    {
        throw std::runtime_error("Creating symbolic links is not supported now.");
    }
    // 确保父目录存在，避免在未创建目录时写入文件失败
    try {
        if (!meta.path.empty()) {
            std::filesystem::create_directories(realpath.parent_path());
        }
    } catch (const std::exception& e) {
        return false;
    }
    std::ofstream ofs(realpath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
    std::unique_ptr<std::vector<std::byte>> data = file.read(CACHE_SIZE);
    while (data && !data->empty())
    {
        if (!ofs.is_open() || !ofs.good())
        {
            ofs.close();
            return false;
        }
        ofs.write(reinterpret_cast<const char*>(data->data()), data->size());
        data = file.read(CACHE_SIZE);
    }
    ofs.flush();
    ofs.close();

    // 尝试设置文件属性，但如果失败不影响恢复操作的成功
    set_file_attributes(meta);
    return true;
}

bool WindowsDevice::write_folder(Folder &folder)
{
    auto meta = folder.get_meta();
    if (const auto folder_path = meta.path; folder_path.empty() || folder_path.string() == "." || folder_path.string() == "./")
    {
        // 根目录不需要创建
        return true;
    }
    meta.type = FileEntityType::Directory;
    const auto realpath = root / folder.get_meta().path;
    // 如果目录已存在，直接返回成功（用于恢复操作）
    if (exists(realpath)) {
        // 检查是否为目录而不是文件
        if (std::filesystem::is_directory(realpath)) {
            return set_file_attributes(meta);
        } else {
            // 如果是文件而不是目录，返回失败
            return false;
        }
    }
    if (!std::filesystem::create_directories(realpath))
        return false;
    return set_file_attributes(meta);
}

bool WindowsDevice::set_file_attributes(const FileEntityMeta& meta)
{
    const auto realpath = root / meta.path;
    if (!exists(realpath))
        return false;

    if (meta.type == FileEntityType::SymbolicLink)
        throw std::runtime_error("Setting attributes for symbolic links is not supported now.");    // TODO: 恢复符号链接

    // set file times
    auto chrono2ft = [](const std::chrono::system_clock::time_point &tp) -> FILETIME {
        FILETIME ft;
        ULARGE_INTEGER ull;
        ull.QuadPart = std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count() / 100 +
                       TICKS_FROM_FILESYSTEM_TO_UTC_EPOCH;
        ft.dwLowDateTime = ull.LowPart;
        ft.dwHighDateTime = ull.HighPart;
        return ft;
    };

    const auto ctime = chrono2ft(meta.creation_time),
    mtime = chrono2ft(meta.modification_time),
    atime = chrono2ft(meta.access_time);

    const HANDLE hFile = CreateFileW(
        realpath.wstring().c_str(),
        FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, // FILE_FLAG_BACKUP_SEMANTICS 允许打开目录
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE)
        return false;
    const auto resultSetFileTime = SetFileTime(hFile, &ctime, &atime, &mtime);
    CloseHandle(hFile);
    if (!resultSetFileTime)
        return false;

    // set file attributes
    DWORD attributes = meta.windows_attributes;
    if (meta.type == FileEntityType::Directory)
        attributes |= FILE_ATTRIBUTE_DIRECTORY;
    else
        attributes &= ~FILE_ATTRIBUTE_DIRECTORY;

    // ReSharper disable once CppDFAConstantConditions
    if (meta.type == FileEntityType::SymbolicLink)
        attributes |= FILE_ATTRIBUTE_REPARSE_POINT;
    else
        attributes &= ~FILE_ATTRIBUTE_REPARSE_POINT;

    if (meta.posix_mode & 0222) // 可写
        attributes &= ~FILE_ATTRIBUTE_READONLY;
    else
        attributes |= FILE_ATTRIBUTE_READONLY;

    if (!SetFileAttributesW(realpath.wstring().c_str(), attributes))
        // 设置文件属性失败
        return false;
    return true;
}


#elif defined(__linux__)
#elif defined(__APPLE__)
#endif
