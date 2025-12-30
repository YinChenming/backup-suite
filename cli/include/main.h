//
// Created by ycm on 25-9-3.
//

#ifndef MAIN_H
#define MAIN_H

#include <string>
#include <vector>
#include <filesystem>
#include "filesystem/system_device.h"
#include "backup/backup_controller.h"
#include "filesystem/compresses_device.h"
#include "filesystem/seven_zip_device.h"

struct CLIOptions {
    bool backup_mode = false;
    bool restore_mode = false;
    std::filesystem::path source_path;
    std::filesystem::path target_path;
    bool use_tar = false;
    bool use_zip = false;
    bool use_7z = false;
    bool use_encryption = false;
    std::string password;
    bool verbose = false;

    // 格式特定选项
    std::string tar_standard = "pax";     // "gnu" 或 "pax"，默认 pax
    std::string zip_encryption = "zipcrypto"; // "zipcrypto" 或 "aes"，默认 zipcrypto

    // 过滤选项
    std::vector<std::string> include_patterns;
    std::vector<std::string> exclude_patterns;
    bool use_regex = false;
    std::vector<std::string> include_extensions;
    std::vector<std::string> exclude_extensions;
    std::string time_after;   // 格式: YYYY-MM-DD 或 YYYY-MM-DD HH:MM:SS
    std::string time_before;
    std::string min_size;     // 支持单位: K, M, G (如 "10M", "1.5G")
    std::string max_size;
    std::vector<std::string> include_users;
    std::vector<std::string> exclude_users;
    std::vector<std::string> include_groups;
    std::vector<std::string> exclude_groups;
};

void print_usage(const char* program_name);
bool parse_arguments(int argc, char* argv[], CLIOptions& options);
bool validate_options(const CLIOptions& options);
int main(int argc, char* argv[]);

#endif //MAIN_H
