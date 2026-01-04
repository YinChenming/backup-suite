# BackupSuite

一个功能强大的备份工具套件，支持多种备份格式和操作。

## 项目结构

```text
.
├── cli/             # 命令行界面模块
├── core/            # 核心功能模块
├── gui/             # 图形用户界面模块（Qt）
├── tests/           # 测试代码
├── test_data/       # 测试数据目录
├── CMakeLists.txt   # CMake主配置文件
├── CMakePresets.json # CMake配置预设
├── vcpkg.json       # vcpkg依赖管理
└── README.md        # 项目说明文档
```

## 依赖项

- C++17或更高版本
- CMake 3.23或更高版本（CI使用 CMake 4.1）
- vcpkg 包管理器（自动安装 gtest、libarchive、sqlite3 等）
- Qt 6.8.x（仅 GUI；CI 通过 `install-qt-action` 安装 `win64_msvc2022_64`）
- NSIS（仅用于生成 Windows 安装包，CI 通过 Chocolatey 安装）

## 构建与打包

### 常用预设（本地）

- Visual Studio Debug：

```powershell
cmake --preset vs-debug
cmake --build --preset vs-debug --config Debug
```

- Visual Studio Release + 打包：

```powershell
cmake --preset vs-release
cmake --build --preset vs-release --config Release
cmake --build --preset vs-release --target package --config Release
```

构建产物默认位于 `cmake-build-*-visual-studio/bin`，其中：

- `bin/cli/backup_suite_cli.exe`
- `bin/gui/backup_suite_gui.exe`
- `bin/tests/core_tests.exe`
- 公共运行时和 `backup_suite_core.dll` 会复制到上述目录以及 `bin/` 根目录。

### CI（GitHub Actions）

Release 工作流（`.github/workflows/release.yml`）在推送 `v*` tag 时执行：

1) Checkout + MSVC 环境
1) 安装 Qt 6.8.3（MSVC 2022 x64）
1) 安装 NSIS（用于生成安装包）
1) 缓存并安装 vcpkg 依赖
1) `cmake --preset vs-release`
1) `cmake --build --preset vs-release --config Release`
1) `cmake --build --preset vs-release --target package --config Release`
1) 从 tag 解析版本号并上传生成的 `BackupSuite-<version>-win64.zip/.exe` 作为 Release 资产

## 7-Zip 集成

- CLI 会自动检测与可执行同目录（或已配置路径）下的 `7z.exe/7za.exe`。缺失时在 `--help` 中给出警告。
- 本地运行时，可将官方 7-Zip 发行版放入 `third_party/7-Zip` 或直接与可执行同目录并重新运行。

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
backup_suite_cli.exe -z --include "subdir" source/ backup.zip

# 只备份以 "sub" 开头的路径
backup_suite_cli.exe -z --include "sub*" source/ backup.zip

# 排除所有 "subdir" 相关的文件
backup_suite_cli.exe -z --exclude "sub*" source/ backup.zip

# 使用正则表达式过滤
backup_suite_cli.exe -z --regex --include "^(test|file)" source/ backup.zip
```

#### 2. 文件扩展名过滤 📝

根据文件后缀名过滤备份文件，支持 `.ext` 和 `ext` 两种格式。

**使用示例**:

```bash
# 只备份 C++ 源文件
backup_suite_cli.exe -z --include-ext .cpp --include-ext .h source/ backup.zip

# 排除日志和临时文件
backup_suite_cli.exe -z --exclude-ext .log --exclude-ext .tmp source/ backup.zip
```

#### 3. 文件大小过滤 📊

根据文件大小范围过滤文件，支持多种单位：`B`、`K`、`M`、`G`、`T`。

**使用示例**:

```bash
# 只备份 10KB 到 100MB 之间的文件
backup_suite_cli.exe -z --min-size 10K --max-size 100M source/ backup.zip

# 只备份大于 1MB 的文件
backup_suite_cli.exe -z --min-size 1M source/

# 只备份小于 100 字节的文件
backup_suite_cli.exe -z --max-size 100B source/ backup.zip
```

#### 4. 修改时间过滤 📅

根据文件修改时间过滤文件，支持日期格式：`YYYY-MM-DD` 或 `YYYY-MM-DD HH:MM:SS`。

**使用示例**:

```bash
# 只备份指定日期之后修改的文件
backup_suite_cli.exe -z --after 2025-12-29 source/ backup.zip

# 只备份指定日期之前修改的文件
backup_suite_cli.exe -z --before 2025-12-31 source/ backup.zip

# 备份指定日期范围内修改的文件
backup_suite_cli.exe -z --after 2025-12-25 --before 2025-12-31 source/ backup.zip
```

#### 5. 文件权限过滤 🔐

根据 POSIX 权限位过滤文件（格式为 8 进制权限码）。

**使用示例**:

```bash
# 只备份所有者可读的文件
backup_suite_cli.exe -z --required-permissions 400 source/ backup.zip

# 排除其他用户可写的文件
backup_suite_cli.exe -z --excluded-permissions 002 source/ backup.zip
```

#### 6. 用户和组所有权过滤 👤

根据文件所有者的用户名或组名过滤。

**使用示例**:

```bash
# 只备份指定用户的文件
backup_suite_cli.exe -z --include-user john source/ backup.zip

# 排除指定用户的文件
backup_suite_cli.exe -z --exclude-user nobody source/ backup.zip

# 只备份指定组的文件
backup_suite_cli.exe -z --include-group admin source/ backup.zip
```

### 组合过滤示例

**示例 1: 智能代码备份**

```bash
backup_suite_cli.exe -7z \
  --include-ext .cpp --include-ext .h \
  --exclude "*test*" \
  --min-size 100 \
  source/ backup.7z
```

**示例 2: 增量备份（按修改时间）**

```bash
backup_suite_cli.exe -7z --after 2025-12-25 source/ incremental_backup.7z
```

**示例 3: 日志备份**

```bash
backup_suite_cli.exe -z --include-ext .log --min-size 1K --after 2025-12-23 source/ logs_backup.zip
```

**示例 4: 清理旧文件前的备份**

```bash
backup_suite_cli.exe -z --before 2025-12-01 source/ old_files_backup.zip
```

### 过滤性能特性

- ⚡ **快速过滤**: 过滤逻辑在扫描文件时实时应用
- 💾 **内存高效**: 不需要预先加载所有文件列表
- 🔄 **增量支持**: 可以与时间过滤配合实现增量备份

## 命令行快速验证（7Z备份与恢复）

以下脚本在 Windows PowerShell 下执行，用于快速验证命令行版本是否支持将根目录中的测试数据备份到 7z 并从 7z 恢复到空目录。

- 可执行文件：`cmake-build-debug-visual-studio/bin/cli/backup_suite_cli.exe`
- 测试数据源：`test_data/source`
- 恢复目标：必须是空目录（脚本会自动创建临时目录）

查看 CLI 帮助（并在缺失 7z 时看到警告）：

```powershell
& "D:\YCM\C++\uestc\BackupSuite\cmake-build-debug-visual-studio\bin\cli\backup_suite_cli.exe" --help
```

一次性完成 7z 备份与恢复验证：

```powershell
$exe = "D:\YCM\C++\uestc\BackupSuite\cmake-build-debug-visual-studio\bin\cli\backup_suite_cli.exe"
$src = "D:\YCM\C++\uestc\BackupSuite\test_data\source"
$work = Join-Path $env:TEMP ("bs_cli_7z_test_" + [System.Guid]::NewGuid().ToString("N"))
New-Item -Path $work -ItemType Directory -Force | Out-Null
$archive = Join-Path $work "backup.7z"
$restore = Join-Path $work "restore"

& $exe -7z -v $src $archive
Write-Host "Archive exists:" (Test-Path $archive)

New-Item -Path $restore -ItemType Directory -Force | Out-Null
& $exe -r -7z -v $archive $restore

Get-ChildItem -Recurse $restore | Select-Object -ExpandProperty FullName
```

可选：验证加密 7z（运行时会提示输入密码）：

```powershell
& $exe -7z -e -p mypassword -v $src $archive
& $exe -r -7z -v $archive $restore
```

## 发行打包（本地）

- 预设 `vs-release` 已启用 vcpkg AppLocal 拷贝，生成包名为 `BackupSuite-<VERSION>-win64.(zip|exe)`，位于
  `cmake-build-release-visual-studio/`。
- 若需要自定义版本，可在打包前设置环境变量 `BACKUPSUITE_VERSION`。

```powershell
cmake --preset vs-release
cmake --build --preset vs-release --config Release
cmake --build --preset vs-release --target package --config Release
```

## 测试说明

项目包含全面的测试套件，使用 Google Test 编写。执行方式：

```powershell
cmake --build --preset vs-debug --target core_tests --config Debug
& "cmake-build-debug-visual-studio/bin/tests/core_tests.exe"
```

## 许可证

本项目采用 **GNU GPL v3** 许可协议，详见仓库中的 `LICENSE` 文件。
