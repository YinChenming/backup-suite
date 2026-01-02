//
// Created by ycm on 2025/12/27.
//
#include "utils/tmpfile.h"

#include <random>
#include <sstream>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace tmpfile_utils;
namespace fs = std::filesystem;

static fs::path tmpfile_path;
static fs::path get_temp_path()
{
    if (!tmpfile_path.empty()) return tmpfile_path;
    try
    {
        tmpfile_path = fs::temp_directory_path();
        if (!fs::exists(tmpfile_path))
        {
            fs::create_directory(tmpfile_path);
        }
        return tmpfile_path;
    } catch ([[maybe_unused]] const std::exception& e)
    {
    }
    // 尝试从环境变量中获取路径
#ifdef _WIN32
    // 通过GetTempPath获取临时目录
    char buffer[MAX_PATH];
    if (const DWORD len = GetTempPathA(MAX_PATH, buffer); len > 0 && len < MAX_PATH)
    {
        tmpfile_path = fs::path(buffer);
        if (!fs::exists(tmpfile_path))
        {
            fs::create_directory(tmpfile_path);
        }
        return tmpfile_path;
    }
#elif defined(__linux__) || defined(__APPLE__)
    const char tmp_env_names[][] = {
        "TMPDIR",
        "TMP",
        "TEMP",
        "TEMPDIR"
    };
    for (const auto & env_name : tmp_env_names)
    {
        const char* env_value = std::getenv(env_name);
        if (env_value != nullptr)
        {
            tmpfile_path = fs::path(env_value);
            if (!fs::exists(tmpfile_path))
            {
                fs::create_directory(tmpfile_path);
            }
            return tmpfile_path;
        }
    }
#endif
    // 最后尝试使用当前目录
    tmpfile_path = fs::current_path() / "cache";
    if (!fs::exists(tmpfile_path))
    {
        fs::create_directory(tmpfile_path);
    }
    return tmpfile_path;
}
static fs::path gen_tmpfile_path()
{
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    if (const UINT ret = GetTempFileNameW(get_temp_path().wstring().c_str(), L"ycm", 0, buffer); ret != 0)
    {
        // GetTempFileNameW 保证文件存在,会创建空文件占位
        return buffer;
    }
#elif defined(__linux__) || defined(__APPLE__)
    std::string template_str = (get_temp_path() / "ycmXXXXXX").string();
    std::vector<char> tmpl(template_str.begin(), template_str.end());
    tmpl.push_back('\0');
    const int fd = mkstemp(tmpl.data());
    if (fd != -1)
    {
        close(fd);
        return tmpl.data();
    }
#endif
    // 自行生成一个文件名
    for (size_t i = 0; i < 100; ++i)
    {
        static std::mt19937 gen(std::random_device{}());
        static constexpr char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz";

        std::stringstream ss;
        ss << "ycm";
        for (int j = 0; j < 8; ++j) {
            ss << charset[gen() % (sizeof(charset) - 1)];
        }
        ss << ".tmp";
        if (const fs::path candidate = get_temp_path() / ss.str(); !fs::exists(candidate))
        {
            std::ofstream ofs(candidate);
            ofs.close();
            return candidate;
        }
    }
    throw std::runtime_error("Failed to create temporary file");
}

std::unique_ptr<TmpFile> TmpFile::create()
{
    return std::make_unique<TmpFile>(gen_tmpfile_path());
}
