# BackupSuite

一个功能强大的备份工具套件，支持多种备份格式和操作。

## 项目结构

```text
.
├── cli/             # 命令行界面模块
├── core/            # 核心功能模块
├── gui/             # 图形用户界面模块
├── tests/           # 测试代码
├── test/            # 测试数据目录
├── CMakeLists.txt   # CMake主配置文件
├── CMakePresets.json # CMake配置预设
├── vcpkg.json       # vcpkg依赖管理
└── README.md        # 项目说明文档
```

## 依赖项

- C++17或更高版本
- CMake 3.19或更高版本
- vcpkg包管理器
- 第三方库（通过vcpkg自动安装）：
  - gtest（用于测试）
  - libarchive（用于压缩功能）
  - sqlite3（用于数据库操作）

## 构建说明

### 使用Visual Studio配置

#### 1. 配置项目

```powershell
cmake --preset vs-debug
```

#### 2. 构建项目

```powershell
cmake --build --preset vs-debug
```

#### 3. 运行测试

```powershell
cmake-build-debug-visual-studio\bin\core_tests.exe
```

### 使用MinGW配置

#### 1. 配置项目

```powershell
cmake --preset mingw-debug
```

#### 2. 构建项目

```powershell
cmake --build --preset mingw-debug
```

#### 3. 运行测试

```powershell
cmake-build-debug\bin\core_tests.exe
```

## 7-Zip 集成（可选）

项目支持在构建时自动拉取并集成 7z 归档相关源码，以实现将来在 `SevenZipDevice` 中读写 7z（含 LZMA/LZMA2、可选 AES）。

- 默认使用 p7zip 源码：仓库 `https://github.com/jinfeihan57/p7zip.git`，版本标签 `v17.05`。
- 固定 vendor 路径：`${repo}/third_party/p7zip`。配置时若该目录不存在，会自动使用 Git 克隆到该路径；若已存在则直接使用本地源码。

常用命令（无需额外预设，默认启用 7z 集成）：

```powershell
cmake --preset vs-debug
cmake --build --preset vs-debug
```

若在线拉取失败，你可以手动下载 p7zip 源码到 `third_party/p7zip` 后重新配置：

```powershell
git clone --depth 1 --branch v17.05 https://github.com/jinfeihan57/p7zip.git third_party/p7zip
cmake --preset vs-debug
cmake --build --preset vs-debug
```

说明：当前版本在配置阶段获取 p7zip 源码并将其编译为内部静态库，随后逐步接入 `SevenZipDevice` 的实际读写；加密（AES）优先使用 7‑Zip 自带实现，必要时可选用 OpenSSL（自动检测）。

默认策略：`SevenZipDevice` 默认压缩为 LZMA2，默认不加密；可通过接口切换压缩（LZMA/LZMA2/Deflate）与加密（AES128/192/256/None）。

许可证提示：p7zip/7‑Zip 包含 LGPL/GPL 组件，请根据使用方式（尤其是静态/动态链接）遵守相应条款并在发布时附带必要的 LICENSE/NOTICE 信息。

### 编译要点（Windows/MSVC）

- 依赖：CMake ≥ 3.20，MSVC 2022，vcpkg（用于 gtest、sqlite3、libarchive；7‑Zip 源码不通过 vcpkg）
- 统一输出目录：所有可执行与动态库产物在 `build/bin` 下，便于测试加载 dll
- 7z 集成默认开启，无需单独预设；若你希望禁用，可在配置时传 `-DBACKUPSUITE_ENABLE_7Z=OFF`。

- OpenSSL 可选：用于未来 AES 集成。若系统未安装，构建会跳过 OpenSSL 链接，但不影响编译。
- 线程：默认以 `_7ZIP_ST` 单线程编译 p7zip，减少额外依赖。如需多线程，可去掉该编译定义并视情况引入线程库。

### 常见问题

- “无法拉取 p7zip 源码”：检查外网连通性或改用 `BACKUPSUITE_USE_LOCAL_7Z=ON` 指向本地源码
- “链接缺失符号”：p7zip 不同版本目录结构略有差异，可在 `core/CMakeLists.txt` 中精简或补充源文件集合
- “OpenSSL 未找到”：可忽略或自行安装 OpenSSL 后重新配置

## 项目模块说明

### core模块

包含项目的核心功能，如文件系统操作、备份管理、压缩功能等。

### cli模块

提供命令行界面，允许用户通过命令行使用BackupSuite的功能。

### gui模块

提供图形用户界面，允许用户通过可视化界面使用BackupSuite的功能。

## 文件过滤功能 🔍

### 功能概述

BackupSuite 提供强大的文件过滤系统，支持在备份操作中根据多个维度智能过滤文件。所有过滤器仅在备份时应用，恢复操作不受影响。

### 支持的过滤器类型

#### 1. 路径模式过滤 📁
根据文件路径进行包含或排除过滤，支持三种模式：
- **简单字符串**: `--include "subdir"` - 匹配包含该字符串的所有路径
- **通配符**: `--include "sub*"` - 使用 `*` 和 `?` 通配符
- **正则表达式**: `--regex --include "^(test|file)"` - 完整正则表达式

**使用示例**:
```bash
# 只备份 subdir 及其内容
backup_suite_cli.exe -7z --include "subdir" source/ backup.7z

# 只备份以 "sub" 开头的路径
backup_suite_cli.exe -7z --include "sub*" source/ backup.7z

# 排除所有 "subdir" 相关的文件
backup_suite_cli.exe -7z --exclude "sub*" source/ backup.7z

# 使用正则表达式过滤
backup_suite_cli.exe -7z --regex --include "^(test|file)" source/ backup.7z
```

#### 2. 文件扩展名过滤 📝
根据文件后缀名过滤备份文件，支持 `.ext` 和 `ext` 两种格式。

**使用示例**:
```bash
# 只备份 C++ 源文件
backup_suite_cli.exe -7z --include-ext .cpp --include-ext .h source/ backup.7z

# 排除日志和临时文件
backup_suite_cli.exe -7z --exclude-ext .log --exclude-ext .tmp source/ backup.7z
```

#### 3. 文件大小过滤 📊
根据文件大小范围过滤文件，支持多种单位：`B`、`K`、`M`、`G`、`T`。

**使用示例**:
```bash
# 只备份 10KB 到 100MB 之间的文件
backup_suite_cli.exe -7z --min-size 10K --max-size 100M source/ backup.7z

# 只备份大于 1MB 的文件
backup_suite_cli.exe -7z --min-size 1M source/ backup.7z

# 只备份小于 100 字节的文件
backup_suite_cli.exe -7z --max-size 100B source/ backup.7z
```

#### 4. 修改时间过滤 📅
根据文件修改时间过滤文件，支持日期格式：`YYYY-MM-DD` 或 `YYYY-MM-DD HH:MM:SS`。

**使用示例**:
```bash
# 只备份指定日期之后修改的文件
backup_suite_cli.exe -7z --after 2025-12-29 source/ backup.7z

# 只备份指定日期之前修改的文件
backup_suite_cli.exe -7z --before 2025-12-31 source/ backup.7z

# 备份指定日期范围内修改的文件
backup_suite_cli.exe -7z --after 2025-12-25 --before 2025-12-31 source/ backup.7z
```

#### 5. 文件权限过滤 🔐
根据 POSIX 权限位过滤文件（格式为 8 进制权限码）。

**使用示例**:
```bash
# 只备份所有者可读的文件
backup_suite_cli.exe -7z --required-permissions 400 source/ backup.7z

# 排除其他用户可写的文件
backup_suite_cli.exe -7z --excluded-permissions 002 source/ backup.7z
```

#### 6. 用户和组所有权过滤 👤
根据文件所有者的用户名或组名过滤。

**使用示例**:
```bash
# 只备份指定用户的文件
backup_suite_cli.exe -7z --include-user john source/ backup.7z

# 排除指定用户的文件
backup_suite_cli.exe -7z --exclude-user nobody source/ backup.7z

# 只备份指定组的文件
backup_suite_cli.exe -7z --include-group admin source/ backup.7z
```

### 组合过滤示例

**示例 1: 智能代码备份**
```bash
# 备份所有代码文件且大于 100 字节，但排除测试文件
backup_suite_cli.exe -7z \
  --include-ext .cpp --include-ext .h \
  --exclude "*test*" \
  --min-size 100 \
  source/ backup.7z
```

**示例 2: 增量备份（按修改时间）**
```bash
# 备份本周修改的文件
backup_suite_cli.exe -7z \
  --after 2025-12-25 \
  source/ incremental_backup.7z
```

**示例 3: 日志备份**
```bash
# 备份所有大于 1KB 的 .log 文件且在过去 7 天内修改
backup_suite_cli.exe -7z \
  --include-ext .log \
  --min-size 1K \
  --after 2025-12-23 \
  source/ logs_backup.7z
```

**示例 4: 清理旧文件前的备份**
```bash
# 备份所有旧文件（2025-12-01 之前修改）
backup_suite_cli.exe -7z \
  --before 2025-12-01 \
  source/ old_files_backup.7z
```

### 过滤性能特性

- ⚡ **快速过滤**: 过滤逻辑在扫描文件时实时应用
- 💾 **内存高效**: 不需要预先加载所有文件列表
- 🔄 **增量支持**: 可以与时间过滤配合实现增量备份

## 测试说明

项目包含全面的测试套件，使用Google Test框架编写。测试覆盖了核心功能的各个方面，包括文件系统操作、压缩功能等。

## 许可证

[在此添加许可证信息]

## 命令行快速验证（7Z备份与恢复）

以下脚本在 Windows PowerShell 下执行，用于快速验证命令行版本是否支持将根目录中的测试数据备份到 7z 并从 7z 恢复到空目录。

- 可执行文件：构建后位于 `cmake-build-debug-visual-studio/bin/backup_suite_cli.exe`
- 测试数据源：`test_data/source`
- 恢复目标：必须是空目录（脚本会自动创建临时目录）

查看 CLI 帮助（确认 `-7z`、`-r`、`-e`、`-p` 等参数）：

```powershell
& "D:\YCM\C++\uestc\BackupSuite\cmake-build-debug-visual-studio\bin\backup_suite_cli.exe" --help
```

一次性完成 7z 备份与恢复验证（自动创建临时工作目录并列出恢复结果）：

```powershell
$exe = "D:\YCM\C++\uestc\BackupSuite\cmake-build-debug-visual-studio\bin\backup_suite_cli.exe"
$src = "D:\YCM\C++\uestc\BackupSuite\test_data\source"
$work = Join-Path $env:TEMP ("bs_cli_7z_test_" + [System.Guid]::NewGuid().ToString("N"))
New-Item -Path $work -ItemType Directory -Force | Out-Null
$archive = Join-Path $work "backup.7z"
$restore = Join-Path $work "restore"

# 备份到 7z（包含详细日志）
& $exe -7z -v $src $archive
Write-Host "Archive exists:" (Test-Path $archive)

# 恢复到一个全新空目录（必须为空）
New-Item -Path $restore -ItemType Directory -Force | Out-Null
& $exe -r -7z -v $archive $restore

# 列出恢复的文件与目录
Get-ChildItem -Recurse $restore | Select-Object -ExpandProperty FullName
```

可选：验证加密 7z（将 `-e -p mypassword` 加入备份与恢复命令，并在恢复时输入相同密码）：

```powershell
& $exe -7z -e -p mypassword -v $src $archive
& $exe -r -7z -v $archive $restore   # 运行时会提示输入密码
```

## 发行打包（本地）

使用 CMake Presets（已内置 vcpkg 与依赖 DLL 的本地拷贝配置），生成可发布的 ZIP 包，仅含 CLI 与必要运行时。

可选设置版本号（不设置时默认使用 `project(BackupSuite VERSION …)` 的版本）：

```powershell
$env:BACKUPSUITE_VERSION = "0.1.1"   # 可选：覆盖包版本号
```

Visual Studio (Release) 打包：

```powershell
cmake --preset vs-release
cmake --build --preset vs-release --target backup_suite_cli
cmake --install cmake-build-release-visual-studio --config Release --prefix "$env:TEMP/backup_suite_install"
cpack -C Release -G ZIP --config "cmake-build-release-visual-studio/CPackConfig.cmake"
```

MinGW/Ninja (Release) 打包：

```powershell
cmake --preset mingw-release
cmake --build --preset mingw-release --target backup_suite_cli
cmake --install cmake-build-release --prefix "$env:TEMP/backup_suite_install"
cpack -C Release -G ZIP --config "cmake-build-release/CPackConfig.cmake"
```

注意：
- 预设已启用 `VCPKG_APPLOCAL_DEPS=ON`，vcpkg 会把依赖 DLL 拷贝到 `bin/`，安装和打包时会被包含到 ZIP。
- 若 CI/机器 vcpkg 安装在非默认路径，请确保环境变量 `VCPKG_ROOT` 已设置。
- 生成包文件名为 `BackupSuite-<VERSION>-win64.zip`。
