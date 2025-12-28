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

struct CLIOptions {
    bool backup_mode = false;
    bool restore_mode = false;
    std::filesystem::path source_path;
    std::filesystem::path target_path;
    bool use_tar = false;
    bool use_zip = false;
    bool use_encryption = false;
    std::string password;
    bool verbose = false;
};

void print_usage(const char* program_name);
bool parse_arguments(int argc, char* argv[], CLIOptions& options);
bool validate_options(const CLIOptions& options);
int main(int argc, char* argv[]);

#endif //MAIN_H
