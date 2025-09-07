//
// Created by ycm on 25-9-4.
//

#ifndef SYSTEM_DEVICE_H
#define SYSTEM_DEVICE_H

#include <utility>
#ifdef _WIN32
#include <windows.h>
#include <Aclapi.h> // For GetSecurityInfo
#include <sddl.h>   // For SID-related functions if needed, though LookupAccountSid is sufficient here
#include <cwchar>

#elif __linux__

#else
#error "Unsupported platform"
static_assert(false, "Unsupported platform");
#endif

#include "device.h"
#include "utils/time_converter.h"


#ifdef _WIN32
// Windows FILETIME epoch starts from January 1, 1601
// Unix epoch starts from January 1, 1970
// The difference is 11644473600 seconds
// FILETIME -> Unix time: (filetime - 116444736000000000) / 10000000
const unsigned long long TICKS_FROM_FILESYSTEM_TO_UTC_EPOCH = 116444736000000000ULL;

class BACKUP_SUITE_API WindowsDevice final : public PhysicalDevice
{
    std::filesystem::path root;
public:
    explicit WindowsDevice(std::filesystem::path path): root(std::move(path)) {}
    [[nodiscard]] std::filesystem::path get_root() const { return root; }
    [[nodiscard]] std::unique_ptr<Folder> get_folder(const std::filesystem::path& path) const override;
    [[nodiscard]] std::unique_ptr<ReadableFile> get_file(const std::filesystem::path& path) const override;
    [[nodiscard]] std::unique_ptr<std::ifstream> get_file_stream(const std::filesystem::path& path) const override;
    [[nodiscard]] std::unique_ptr<FileEntityMeta> get_meta(const std::filesystem::path& path) const override;
    [[nodiscard]] bool exists(const std::filesystem::path& path) const override;
    bool write_file(const ReadableFile&) override;
    bool write_folder(const Folder &folder) override;
    static std::wstring sid2name(const PSID sid) {
        if (!sid) {
            return L"";
        }

        DWORD name_len = 0; // 不包括结尾的\0
        DWORD domain_len = 0;   // 不包括结尾的\0
        SID_NAME_USE sid_use;

        // 第一次调用，获取所需的缓冲区大小
        LookupAccountSidW(nullptr, sid, nullptr, &name_len, nullptr, &domain_len, &sid_use);

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return L"";
        }

        std::wstring name_buf(name_len, L'\0');
        std::wstring domain_buf(domain_len, L'\0');

        // 第二次调用，获取真正的名称
        if (!LookupAccountSidW(nullptr, sid, &name_buf[0], &name_len, &domain_buf[0], &domain_len, &sid_use)) {
            return L"";
        }
    
        // 如果有域名，则组合成 "Domain\Name" 格式
        if (domain_len > 1) { // >1 because it includes a nullptr terminator
            return domain_buf.substr(0, domain_len) + L"\\" + name_buf.substr(0, name_len);
        }
        // return name_buf.substr(0, name_len - 1);
        return name_buf;
    }
};

using SystemDevice = WindowsDevice;

class WindowsTempDevice: public PhysicalDevice
{

};

using TempDevice = WindowsTempDevice;

#elif defined __linux__
class BACKUP_SUITE_API LinuxDevice final : public device
{
};

using SystemDevice = LinuxDevice;
#endif

#endif //SYSTEM_DEVICE_H
