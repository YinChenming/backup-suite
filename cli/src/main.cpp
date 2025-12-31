//
// Created by ycm on 25-9-3.
//
#include "main.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] <source_path> <target_path>" << std::endl;
    std::cout << std::endl;
    std::cout << "Basic Options:" << std::endl;
    std::cout << "  -b, --backup          Backup mode (default)" << std::endl;
    std::cout << "  -r, --restore         Restore mode" << std::endl;
    std::cout << "  -t, --tar             Use TAR format" << std::endl;
    std::cout << "  -z, --zip             Use ZIP format" << std::endl;
    std::cout << "  -7z, --7z             Use 7-Zip format" << std::endl;
    std::cout << "  -e, --encrypt         Enable encryption (ZIP/7Z only)" << std::endl;
    std::cout << "  -p, --password PASS   Set password (requires -e)" << std::endl;
    std::cout << "  -v, --verbose         Verbose output" << std::endl;
    std::cout << "  -h, --help            Show this help information" << std::endl;
    std::cout << std::endl;
    std::cout << "Format-specific Options:" << std::endl;
    std::cout << "  --tar-format FORMAT   TAR format: 'pax' or 'gnu' (default: pax)" << std::endl;
    std::cout << "  --zip-encryption TYPE ZIP encryption: 'zipcrypto' or 'aes' (default: zipcrypto)" << std::endl;
    std::cout << std::endl;
    std::cout << "Filter Options (Backup mode only):" << std::endl;
    std::cout << "  --include PATTERN     Include files matching pattern (can be used multiple times)" << std::endl;
    std::cout << "  --exclude PATTERN     Exclude files matching pattern (can be used multiple times)" << std::endl;
    std::cout << "  --regex               Use regex for patterns instead of wildcards" << std::endl;
    std::cout << "  --include-ext EXT     Include files with extension (e.g., .txt, .cpp)" << std::endl;
    std::cout << "  --exclude-ext EXT     Exclude files with extension" << std::endl;
    std::cout << "  --after DATE          Include files modified after date (YYYY-MM-DD)" << std::endl;
    std::cout << "  --before DATE         Include files modified before date (YYYY-MM-DD)" << std::endl;
    std::cout << "  --min-size SIZE       Minimum file size (e.g., 10K, 1M, 1.5G)" << std::endl;
    std::cout << "  --max-size SIZE       Maximum file size" << std::endl;
    std::cout << "  --include-user USER   Include files owned by user" << std::endl;
    std::cout << "  --exclude-user USER   Exclude files owned by user" << std::endl;
    std::cout << "  --include-group GROUP Include files owned by group" << std::endl;
    std::cout << "  --exclude-group GROUP Exclude files owned by group" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  Basic backup:" << std::endl;
    std::cout << "    " << program_name << " -z /path/to/source /path/to/backup.zip" << std::endl;
    std::cout << std::endl;
    std::cout << "  TAR backup with GNU format:" << std::endl;
    std::cout << "    " << program_name << " -t --tar-format gnu /path/to/source backup.tar" << std::endl;
    std::cout << std::endl;
    std::cout << "  ZIP backup with AES encryption:" << std::endl;
    std::cout << "    " << program_name << " -z -e -p mypass --zip-encryption aes /src backup.zip" << std::endl;
    std::cout << std::endl;
    std::cout << "  Backup only .cpp and .h files:" << std::endl;
    std::cout << "    " << program_name << " -z --include-ext .cpp --include-ext .h /src backup.zip" << std::endl;
    std::cout << std::endl;
    std::cout << "  Backup excluding temporary files:" << std::endl;
    std::cout << "    " << program_name << R"( -7z --exclude "*.tmp" --exclude "*.log" /src backup.7z)" << std::endl;
    std::cout << std::endl;
    std::cout << "  Backup files modified in last week (> 1MB):" << std::endl;
    std::cout << "    " << program_name << " -z --after 2025-12-23 --min-size 1M /src backup.zip" << std::endl;
    std::cout << std::endl;
    std::cout << "  Restore:" << std::endl;
    std::cout << "    " << program_name << " -r -z /path/to/backup.zip /path/to/restore" << std::endl;
}

bool parse_arguments(const int argc, char* argv[], CLIOptions& options) {
    // 默认是备份模式
    options.backup_mode = true;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return false; // 返回 false 表示应该退出
        }

        if (arg == "-b" || arg == "--backup") {
            options.backup_mode = true;
            options.restore_mode = false;
        } else if (arg == "-r" || arg == "--restore") {
            options.restore_mode = true;
            options.backup_mode = false;
        } else if (arg == "-t" || arg == "--tar") {
            options.use_tar = true;
            options.use_zip = false;
            options.use_7z = false;
        } else if (arg == "-z" || arg == "--zip") {
            options.use_zip = true;
            options.use_tar = false;
            options.use_7z = false;
        } else if (arg == "-7z" || arg == "--7z") {
            options.use_7z = true;
            options.use_tar = false;
            options.use_zip = false;
        } else if (arg == "-e" || arg == "--encrypt") {
            options.use_encryption = true;
        } else if (arg == "-p" || arg == "--password") {
            if (i + 1 < argc) {
                options.password = argv[++i];
            } else {
                std::cerr << "Error: --password option requires a password" << std::endl;
                return false;
            }
        } else if (arg == "-v" || arg == "--verbose") {
            options.verbose = true;
        } else if (arg == "--include") {
            if (i + 1 < argc) {
                options.include_patterns.emplace_back(argv[++i]);
            } else {
                std::cerr << "Error: --include requires a pattern" << std::endl;
                return false;
            }
        } else if (arg == "--exclude") {
            if (i + 1 < argc) {
                options.exclude_patterns.emplace_back(argv[++i]);
            } else {
                std::cerr << "Error: --exclude requires a pattern" << std::endl;
                return false;
            }
        } else if (arg == "--regex") {
            options.use_regex = true;
        } else if (arg == "--include-ext") {
            if (i + 1 < argc) {
                options.include_extensions.emplace_back(argv[++i]);
            } else {
                std::cerr << "Error: --include-ext requires an extension" << std::endl;
                return false;
            }
        } else if (arg == "--exclude-ext") {
            if (i + 1 < argc) {
                options.exclude_extensions.emplace_back(argv[++i]);
            } else {
                std::cerr << "Error: --exclude-ext requires an extension" << std::endl;
                return false;
            }
        } else if (arg == "--after") {
            if (i + 1 < argc) {
                options.time_after = argv[++i];
            } else {
                std::cerr << "Error: --after requires a date" << std::endl;
                return false;
            }
        } else if (arg == "--before") {
            if (i + 1 < argc) {
                options.time_before = argv[++i];
            } else {
                std::cerr << "Error: --before requires a date" << std::endl;
                return false;
            }
        } else if (arg == "--min-size") {
            if (i + 1 < argc) {
                options.min_size = argv[++i];
            } else {
                std::cerr << "Error: --min-size requires a size" << std::endl;
                return false;
            }
        } else if (arg == "--max-size") {
            if (i + 1 < argc) {
                options.max_size = argv[++i];
            } else {
                std::cerr << "Error: --max-size requires a size" << std::endl;
                return false;
            }
        } else if (arg == "--include-user") {
            if (i + 1 < argc) {
                options.include_users.emplace_back(argv[++i]);
            } else {
                std::cerr << "Error: --include-user requires a username" << std::endl;
                return false;
            }
        } else if (arg == "--exclude-user") {
            if (i + 1 < argc) {
                options.exclude_users.emplace_back(argv[++i]);
            } else {
                std::cerr << "Error: --exclude-user requires a username" << std::endl;
                return false;
            }
        } else if (arg == "--include-group") {
            if (i + 1 < argc) {
                options.include_groups.emplace_back(argv[++i]);
            } else {
                std::cerr << "Error: --include-group requires a group name" << std::endl;
                return false;
            }
        } else if (arg == "--exclude-group") {
            if (i + 1 < argc) {
                options.exclude_groups.emplace_back(argv[++i]);
            } else {
                std::cerr << "Error: --exclude-group requires a group name" << std::endl;
                return false;
            }
        } else if (arg == "--tar-format") {
            if (i + 1 < argc) {
                std::string format = argv[++i];
                if (format == "pax" || format == "gnu") {
                    options.tar_standard = format;
                } else {
                    std::cerr << "Error: --tar-format must be 'pax' or 'gnu'" << std::endl;
                    return false;
                }
            } else {
                std::cerr << "Error: --tar-format requires a format (pax or gnu)" << std::endl;
                return false;
            }
        } else if (arg == "--zip-encryption") {
            if (i + 1 < argc) {
                std::string encryption = argv[++i];
                if (encryption == "zipcrypto" || encryption == "rc4") {
                    options.zip_encryption = encryption;
                } else {
                    std::cerr << "Error: --zip-encryption must be 'zipcrypto' or 'aes'" << std::endl;
                    return false;
                }
            } else {
                std::cerr << "Error: --zip-encryption requires a type (zipcrypto or aes)" << std::endl;
                return false;
            }
        } else if (arg[0] != '-') {
            // 这是一个路径参数
            if (options.source_path.empty()) {
                options.source_path = arg;
            } else if (options.target_path.empty()) {
                options.target_path = arg;
            } else {
                std::cerr << "Error: Too many path parameters" << std::endl;
                return false;
            }
        } else {
            std::cerr << "Error: Unknown option " << arg << std::endl;
            return false;
        }
    }

    return true;
}

bool validate_options(const CLIOptions& options) {
    // Check path parameters
    if (options.source_path.empty() || options.target_path.empty()) {
        std::cerr << "Error: Source path and target path are required" << std::endl;
        return false;
    }

    // Check mode
    if (!options.backup_mode && !options.restore_mode) {
        std::cerr << "Error: Must specify backup mode (-b) or restore mode (-r)" << std::endl;
        return false;
    }

    // Check compression format
    if (!options.use_tar && !options.use_zip && !options.use_7z) {
        std::cerr << "Error: Must specify compression format (-t or -z or -7z)" << std::endl;
        return false;
    }

    // Check encryption options
    if (options.use_encryption && !(options.use_zip || options.use_7z)) {
        std::cerr << "Error: Encryption option (-e) can only be used with ZIP or 7Z format" << std::endl;
        return false;
    }

    if (options.use_encryption && options.password.empty()) {
        std::cerr << "Error: Must specify password (-p) when using encryption option (-e)" << std::endl;
        return false;
    }

    return true;
}

bool prompt_for_password(std::string& password) {
    std::cout << "Enter password (ZIP/7Z): ";
    std::getline(std::cin, password);
    return !password.empty();
}

// 解析文件大小字符串 (支持 K, M, G 单位)
uint64_t parse_size(const std::string& size_str) {
    if (size_str.empty()) return 0;

    double value = 0;
    char unit = 0;
    size_t pos = 0;

    try {
        value = std::stod(size_str, &pos);
    } catch (...) {
        return 0;
    }

    if (pos < size_str.length()) {
        unit = std::toupper(size_str[pos]);
    }

    uint64_t multiplier = 1;
    switch (unit) {
        case 'K': multiplier = 1024; break;
        case 'M': multiplier = 1024 * 1024; break;
        case 'G': multiplier = 1024ULL * 1024 * 1024; break;
        case 'T': multiplier = 1024ULL * 1024 * 1024 * 1024; break;
        default: break;
    }

    return static_cast<uint64_t>(value * multiplier);
}

// 解析日期字符串 (YYYY-MM-DD 或 YYYY-MM-DD HH:MM:SS)
std::chrono::system_clock::time_point parse_date(const std::string& date_str) {
    std::tm tm = {};
    std::istringstream ss(date_str);

    // 尝试解析 YYYY-MM-DD
    if (date_str.find(':') == std::string::npos) {
        ss >> std::get_time(&tm, "%Y-%m-%d");
    } else {
        // 尝试解析 YYYY-MM-DD HH:MM:SS
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    }

    if (ss.fail()) {
        return std::chrono::system_clock::time_point{};
    }

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// 将 CLI 选项转换为 BackupConfig
BackupConfig build_backup_config(const CLIOptions& options) {
    BackupConfig config;

    // 路径过滤
    config.include_patterns = options.include_patterns;
    config.exclude_patterns = options.exclude_patterns;
    config.use_regex = options.use_regex;

    // 扩展名过滤
    config.include_extensions = options.include_extensions;
    config.exclude_extensions = options.exclude_extensions;

    // 时间过滤
    if (!options.time_after.empty() || !options.time_before.empty()) {
        config.filter_by_time = true;
        if (!options.time_after.empty()) {
            config.time_after = parse_date(options.time_after);
        } else {
            config.time_after = std::chrono::system_clock::time_point::min();
        }
        if (!options.time_before.empty()) {
            config.time_before = parse_date(options.time_before);
        } else {
            config.time_before = std::chrono::system_clock::time_point::max();
        }
    }

    // 大小过滤
    if (!options.min_size.empty() || !options.max_size.empty()) {
        config.filter_by_size = true;
        config.min_size = parse_size(options.min_size);
        config.max_size = options.max_size.empty() ? UINT64_MAX : parse_size(options.max_size);
    }

    // 用户/组过滤
    config.include_users = options.include_users;
    config.exclude_users = options.exclude_users;
    config.include_groups = options.include_groups;
    config.exclude_groups = options.exclude_groups;

    return config;
}

int main(int argc, char* argv[]) {
    CLIOptions options;

    // Parse command line arguments
    if (!parse_arguments(argc, argv, options)) {
        return 1; // Return 1 for help requests, otherwise return error code
    }

    // Validate options
    if (!validate_options(options)) {
        print_usage(argv[0]);
        return 1;
    }

    try {
        // 为备份模式构建配置
        BackupConfig backup_config;
        if (options.backup_mode) {
            backup_config = build_backup_config(options);
        }
        BackupController controller(backup_config);

        if (options.backup_mode) {
            if (options.verbose) {
                std::cout << "Starting backup operation..." << std::endl;
                std::cout << "Source path: " << options.source_path << std::endl;
                std::cout << "Target path: " << options.target_path << std::endl;
                std::cout << "Format: " << (options.use_tar ? "TAR" : options.use_zip ? "ZIP" : "7Z") << std::endl;
                if (options.use_encryption) {
                    std::cout << "Encryption: Enabled" << std::endl;
                }
                // 显示过滤信息
                if (!options.include_patterns.empty()) {
                    std::cout << "Include patterns: ";
                    for (const auto& p : options.include_patterns) std::cout << p << " ";
                    std::cout << std::endl;
                }
                if (!options.exclude_patterns.empty()) {
                    std::cout << "Exclude patterns: ";
                    for (const auto& p : options.exclude_patterns) std::cout << p << " ";
                    std::cout << std::endl;
                }
                if (!options.include_extensions.empty()) {
                    std::cout << "Include extensions: ";
                    for (const auto& e : options.include_extensions) std::cout << e << " ";
                    std::cout << std::endl;
                }
                if (!options.exclude_extensions.empty()) {
                    std::cout << "Exclude extensions: ";
                    for (const auto& e : options.exclude_extensions) std::cout << e << " ";
                    std::cout << std::endl;
                }
                if (!options.time_after.empty()) {
                    std::cout << "Modified after: " << options.time_after << std::endl;
                }
                if (!options.time_before.empty()) {
                    std::cout << "Modified before: " << options.time_before << std::endl;
                }
                if (!options.min_size.empty()) {
                    std::cout << "Min size: " << options.min_size << std::endl;
                }
                if (!options.max_size.empty()) {
                    std::cout << "Max size: " << options.max_size << std::endl;
                }
            }

            // Check if source path exists
            if (!std::filesystem::exists(options.source_path)) {
                std::cerr << "Error: Source path does not exist: " << options.source_path << std::endl;
                return 1;
            }

            // Create source device
            SystemDevice source_device(options.source_path);

            // Create target device
            if (options.use_tar) {
                TarDevice target_device(options.target_path, TarDevice::Mode::WriteOnly);
                if (!target_device.is_open()) {
                    std::cerr << "Error: Cannot create TAR file: " << options.target_path << std::endl;
                    return 1;
                }

                // 设置TAR标准
                tar::TarStandard tar_std = tar::TarStandard::POSIX_2001_PAX; // 默认PAX
                if (options.tar_standard == "gnu") {
                    tar_std = tar::TarStandard::GNU;
                } else if (options.tar_standard == "pax") {
                    tar_std = tar::TarStandard::POSIX_2001_PAX;
                }
                target_device.set_standard(tar_std);

                if (options.verbose) {
                    std::cout << "Creating TAR backup..." << std::endl;
                    std::cout << "TAR format: " << options.tar_standard << std::endl;
                }

                controller.run_backup(source_device, target_device);
                target_device.close();

                if (options.verbose) {
                    std::cout << "Backup completed!" << std::endl;
                }
            } else if (options.use_zip) {
                std::vector<uint8_t> password_vec;
                if (options.use_encryption) {
                    password_vec.assign(options.password.begin(), options.password.end());
                }

                ZipDevice target_device(options.target_path, ZipDevice::Mode::WriteOnly, password_vec);
                if (!target_device.is_open()) {
                    std::cerr << "Error: Cannot create ZIP file: " << options.target_path << std::endl;
                    return 1;
                }

                if (options.use_encryption) {
                    // 设置加密方法
                    zip::header::ZipEncryptionMethod encryption_method = zip::header::ZipEncryptionMethod::ZipCrypto;
                    if (options.zip_encryption == "rc4") {
                        encryption_method = zip::header::ZipEncryptionMethod::RC4;
                    } else if (options.zip_encryption == "zipcrypto") {
                        encryption_method = zip::header::ZipEncryptionMethod::ZipCrypto;
                    }
                    target_device.set_encryption_method(encryption_method);

                    if (options.verbose) {
                        std::cout << "Using " << options.zip_encryption << " encryption..." << std::endl;
                    }
                }

                if (options.verbose) {
                    std::cout << "Creating ZIP backup..." << std::endl;
                }

                controller.run_backup(source_device, target_device);
                target_device.close();

                if (options.verbose) {
                    std::cout << "Backup completed!" << std::endl;
                }
            } else if (options.use_7z) {
                std::vector<uint8_t> password_vec;
                if (options.use_encryption) {
                    password_vec.assign(options.password.begin(), options.password.end());
                }

                SevenZipDevice target_device(options.target_path, SevenZipDevice::Mode::WriteOnly,
                                             sevenzip::CompressionMethod::LZMA2,
                                             options.use_encryption ? sevenzip::EncryptionMethod::AES256 : sevenzip::EncryptionMethod::None,
                                             password_vec);
                if (!target_device.is_open()) {
                    std::cerr << "Error: Cannot create 7Z file: " << options.target_path << std::endl;
                    return 1;
                }

                if (options.verbose) {
                    std::cout << "Creating 7Z backup..." << std::endl;
                }

                controller.run_backup(source_device, target_device);
                target_device.close();

                if (options.verbose) {
                    std::cout << "Backup completed!" << std::endl;
                }
            }

        } else if (options.restore_mode) {
            if (options.verbose) {
                std::cout << "Restore begin..." << std::endl;
                std::cout << "source dir: " << options.source_path << std::endl;
                std::cout << "dest dir: " << options.target_path << std::endl;
                std::cout << "format: " << (options.use_tar ? "TAR" : options.use_zip ? "ZIP" : "7Z") << std::endl;
            }

            // 恢复操作可以覆盖现有文件，不需要检查目标目录是否为空

            // 创建目标设备
            SystemDevice target_device(options.target_path);

            if (options.use_tar) {
                TarDevice source_device(options.source_path, TarDevice::Mode::ReadOnly);
                if (!source_device.is_open()) {
                    std::cerr << "Error: fail to open TAR file: " << options.source_path << std::endl;
                    return 1;
                }

                if (options.verbose) {
                    std::cout << "Restore from TAR ..." << std::endl;
                }

                bool success = controller.run_restore(source_device, target_device);
                source_device.close();

                if (success) {
                    if (options.verbose) {
                        std::cout << "Success!" << std::endl;
                    }
                } else {
                    std::cerr << "Error: restore failed" << std::endl;
                    return 1;
                }
            } else if (options.use_zip) {
                // 检查ZIP文件是否需要密码
                std::vector<uint8_t> password_vec;
                {
                    ZipDevice temp_device(options.source_path, ZipDevice::Mode::ReadOnly);
                        if (!temp_device.is_open()) {
                            std::cerr << "Error: Cannot open ZIP file: " << options.source_path << std::endl;
                            return 1;
                        }

                    // 如果需要密码但没有提供，提示用户输入
                    if (temp_device.is_invalid_password()) {
                        if (options.password.empty()) {
                            if (!prompt_for_password(options.password)) {
                                std::cerr << "Error: No password provided" << std::endl;
                                return 1;
                            }
                        }
                        password_vec.assign(options.password.begin(), options.password.end());
                    }
                }

                ZipDevice source_device(options.source_path, ZipDevice::Mode::ReadOnly, password_vec);
                if (!source_device.is_open()) {
                    if (source_device.is_invalid_password()) {
                        std::cerr << "Error: Incorrect password or no password provided" << std::endl;
                    } else {
                        std::cerr << "Error: Cannot open ZIP file: " << options.source_path << std::endl;
                    }
                    return 1;
                }

                if (options.verbose) {
                    std::cout << "Restoring from ZIP file..." << std::endl;
                }

                bool success = controller.run_restore(source_device, target_device);
                source_device.close();

                if (success) {
                    if (options.verbose) {
                        std::cout << "Restore completed!" << std::endl;
                    }
                } else {
                    std::cerr << "Error: Restore operation failed" << std::endl;
                    return 1;
                }
            } else if (options.use_7z) {
                // Check if password is provided or needed
                std::vector<uint8_t> password_vec;
                if (!options.password.empty()) {
                    password_vec.assign(options.password.begin(), options.password.end());
                } else {
                    // Try to open without password first
                    SevenZipDevice temp_device(options.source_path, SevenZipDevice::Mode::ReadOnly);
                    if (!temp_device.is_open()) {
                        // File might need a password, prompt for it
                        if (!prompt_for_password(options.password)) {
                            std::cerr << "Error: No password provided" << std::endl;
                            return 1;
                        }
                        password_vec.assign(options.password.begin(), options.password.end());
                    }
                }

                SevenZipDevice source_device(options.source_path, SevenZipDevice::Mode::ReadOnly,
                                             sevenzip::CompressionMethod::LZMA2,
                                             password_vec.empty() ? sevenzip::EncryptionMethod::None : sevenzip::EncryptionMethod::AES256,
                                             password_vec);
                if (!source_device.is_open()) {
                    if (source_device.is_invalid_password()) {
                        std::cerr << "Error: Incorrect password or no password provided" << std::endl;
                    } else {
                        std::cerr << "Error: Cannot open 7Z file: " << options.source_path << std::endl;
                    }
                    return 1;
                }

                if (options.verbose) {
                    std::cout << "Restoring from 7Z file..." << std::endl;
                }

                bool success = controller.run_restore(source_device, target_device);
                source_device.close();

                if (success) {
                    if (options.verbose) {
                        std::cout << "Restore completed!" << std::endl;
                    }
                } else {
                    std::cerr << "Error: Restore operation failed" << std::endl;
                    return 1;
                }
            }
            }

        std::cout << "Operation completed successfully!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
