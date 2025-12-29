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

## 测试说明

项目包含全面的测试套件，使用Google Test框架编写。测试覆盖了核心功能的各个方面，包括文件系统操作、压缩功能等。

## 许可证

[在此添加许可证信息]
