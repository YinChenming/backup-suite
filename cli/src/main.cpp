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

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] <source_path> <target_path>" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -b, --backup          Backup mode (default)" << std::endl;
    std::cout << "  -r, --restore         Restore mode" << std::endl;
    std::cout << "  -t, --tar             Use TAR format" << std::endl;
    std::cout << "  -z, --zip             Use ZIP format" << std::endl;
    std::cout << "  -e, --encrypt         Enable ZIP encryption (requires -z)" << std::endl;
    std::cout << "  -p, --password PASS   Set password (requires -e)" << std::endl;
    std::cout << "  -v, --verbose         Verbose output" << std::endl;
    std::cout << "  -h, --help            Show this help information" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  Backup folder to ZIP:" << std::endl;
    std::cout << "    " << program_name << " -z /path/to/source /path/to/backup.zip" << std::endl;
    std::cout << std::endl;
    std::cout << "  Backup folder to encrypted ZIP:" << std::endl;
    std::cout << "    " << program_name << " -z -e -p mypassword /path/to/source /path/to/backup.zip" << std::endl;
    std::cout << std::endl;
    std::cout << "  Backup folder to TAR:" << std::endl;
    std::cout << "    " << program_name << " -t /path/to/source /path/to/backup.tar" << std::endl;
    std::cout << std::endl;
    std::cout << "  Restore from ZIP to folder:" << std::endl;
    std::cout << "    " << program_name << " -r -z /path/to/backup.zip /path/to/restore" << std::endl;
    std::cout << std::endl;
    std::cout << "  Restore from encrypted ZIP (will prompt for password):" << std::endl;
    std::cout << "    " << program_name << " -r -z /path/to/backup.zip /path/to/restore" << std::endl;
}

bool parse_arguments(int argc, char* argv[], CLIOptions& options) {
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
        } else if (arg == "-z" || arg == "--zip") {
            options.use_zip = true;
            options.use_tar = false;
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
    if (!options.use_tar && !options.use_zip) {
        std::cerr << "Error: Must specify compression format (-t or -z)" << std::endl;
        return false;
    }

    // Check encryption options
    if (options.use_encryption && !options.use_zip) {
        std::cerr << "Error: Encryption option (-e) can only be used with ZIP format" << std::endl;
        return false;
    }

    if (options.use_encryption && options.password.empty()) {
        std::cerr << "Error: Must specify password (-p) when using encryption option (-e)" << std::endl;
        return false;
    }

    return true;
}

bool prompt_for_password(std::string& password) {
    std::cout << "Enter ZIP password: ";
    std::getline(std::cin, password);
    return !password.empty();
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
        BackupController controller;

        if (options.backup_mode) {
            if (options.verbose) {
                std::cout << "Starting backup operation..." << std::endl;
                std::cout << "Source path: " << options.source_path << std::endl;
                std::cout << "Target path: " << options.target_path << std::endl;
                std::cout << "Format: " << (options.use_tar ? "TAR" : "ZIP") << std::endl;
                if (options.use_encryption) {
                    std::cout << "Encryption: Enabled" << std::endl;
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

                if (options.verbose) {
                    std::cout << "Creating TAR backup..." << std::endl;
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
                    target_device.set_encryption_method(zip::header::ZipEncryptionMethod::ZipCrypto);
                    if (options.verbose) {
                        std::cout << "Using ZIP Crypto encryption..." << std::endl;
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
            }

        } else if (options.restore_mode) {
            if (options.verbose) {
                std::cout << "开始恢复操作..." << std::endl;
                std::cout << "源路径: " << options.source_path << std::endl;
                std::cout << "目标路径: " << options.target_path << std::endl;
                std::cout << "格式: " << (options.use_tar ? "TAR" : "ZIP") << std::endl;
            }

            // 恢复操作可以覆盖现有文件，不需要检查目标目录是否为空

            // 创建目标设备
            SystemDevice target_device(options.target_path);

            if (options.use_tar) {
                TarDevice source_device(options.source_path, TarDevice::Mode::ReadOnly);
                if (!source_device.is_open()) {
                    std::cerr << "错误: 无法打开 TAR 文件: " << options.source_path << std::endl;
                    return 1;
                }

                if (options.verbose) {
                    std::cout << "正在从 TAR 文件恢复..." << std::endl;
                }

                bool success = controller.run_restore(source_device, target_device);
                source_device.close();

                if (success) {
                    if (options.verbose) {
                        std::cout << "恢复完成!" << std::endl;
                    }
                } else {
                    std::cerr << "错误: 恢复操作失败" << std::endl;
                    return 1;
                }
            } else if (options.use_zip) {
                // 检查ZIP文件是否需要密码
                std::vector<uint8_t> password_vec;
                {
                    ZipDevice temp_device(options.source_path, ZipDevice::Mode::ReadOnly);
                    if (!temp_device.is_open()) {
                        std::cerr << "错误: 无法打开 ZIP 文件: " << options.source_path << std::endl;
                        return 1;
                    }

                    // 如果需要密码但没有提供，提示用户输入
                    if (temp_device.is_invalid_password()) {
                        if (options.password.empty()) {
                            if (!prompt_for_password(options.password)) {
                                std::cerr << "错误: 未提供密码" << std::endl;
                                return 1;
                            }
                        }
                        password_vec.assign(options.password.begin(), options.password.end());
                    }
                }

                ZipDevice source_device(options.source_path, ZipDevice::Mode::ReadOnly, password_vec);
                if (!source_device.is_open()) {
                    if (source_device.is_invalid_password()) {
                        std::cerr << "错误: 密码不正确或未提供密码" << std::endl;
                    } else {
                        std::cerr << "错误: 无法打开 ZIP 文件: " << options.source_path << std::endl;
                    }
                    return 1;
                }

                if (options.verbose) {
                    std::cout << "正在从 ZIP 文件恢复..." << std::endl;
                }

                bool success = controller.run_restore(source_device, target_device);
                source_device.close();

                if (success) {
                    if (options.verbose) {
                        std::cout << "恢复完成!" << std::endl;
                    }
                } else {
                    std::cerr << "错误: 恢复操作失败" << std::endl;
                    return 1;
                }
            }
        }

        std::cout << "操作成功完成!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "异常错误: " << e.what() << std::endl;
        return 1;
    }
}
