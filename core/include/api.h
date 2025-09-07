//
// Created by ycm on 25-9-3.
//

#pragma once
#ifndef API_H
#define API_H

// 这个宏只在Windows平台上有意义
#ifdef _WIN32
  #ifdef BACKUP_SUITE_CORE_EXPORTS
    // 如果定义了 BACKUP_SUITE_CORE_EXPORTS, 说明我们正在构建 DLL 本身
    // 这时需要将符号导出 (export)
    #define BACKUP_SUITE_API __declspec(dllexport)
  #else
    // 否则, 说明我们是 DLL 的使用者 (比如 tests, cli 项目)
    // 这时需要从 DLL 中将符号导入 (import)
    #define BACKUP_SUITE_API __declspec(dllimport)
  #endif
#else
  // 在非 Windows 平台 (如 Linux, macOS)，这个机制不同，通常不需要
  #define BACKUP_SUITE_API
#endif

#endif //API_H
