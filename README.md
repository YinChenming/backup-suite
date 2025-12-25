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
