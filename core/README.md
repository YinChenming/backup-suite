# 开发环境

- ## *Windows* 平台

  *Windows*平台开发环境如下:

    - 版本: `Windows 11 家庭中文版`
    - 版本号: `24H2`
    - 安装日期: `2025.1.27`
    - 操作系统版本: `26100.4946`
    - 体验: `Windows 功能体验包 1000.26100.197.0`

  `MSVC 2022`信息:

    - 版本: `19.44.35215`

  `MinGW-w64`信息:

    - 版本: `(MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r2) 14.2.0`

  `core`中所有代码均通过`MSVC`和`MinGW-w64`成功编译，并且在两种编译器下均通过了所有测试。\
  在*Windows*平台中的文件元数据会调用`windows.h`中的API来获取。由于其本身的兼容性问题，不排除**Windows10**以下的操作系统不支持部分API。

# `FileEntity`

## `Meta` 数据

```c++
enum class FileEntityType
{
    RegularFile,
    Directory,
    SymbolicLink,
    Fifo,
    Socket,
    BlockDevice,
    CharacterDevice,
    Unknown
};
```

其中，*Windows*平台只有`RegularFile`、`Directory`和`SymbolicLink`三种类型，特别的，*Windows*平台的`SymbolicLink`
只有软链接，其他的链接（如硬链接、目录链接、快捷方式）本质上都是`RegularFile`，只能通过文件内容来区分。

```c++
struct FileEntityMeta
{
    std::filesystem::path path;
    FileEntityType type = FileEntityType::Unknown;
    size_t size = 0;

    std::chrono::system_clock::time_point creation_time;
    std::chrono::system_clock::time_point modification_time;
    std::chrono::system_clock::time_point access_time;

    // POSIX 特有元数据
    uint32_t posix_mode = 0;
    uint32_t uid = 0;
    uint32_t gid = 0;

    std::string user_name, group_name;

    // Windows 特有元数据
    uint32_t windows_attributes = 0;

    // 特殊文件元数据
    std::filesystem::path symbolic_link_target;
    uint32_t device_major = 0;
    uint32_t device_minor = 0;
};
```

其中部分数据是只有*Windows*平台或*Unix*平台才有的，理论上我们的程序会尽力地对应。\
在*Windows*平台上，会根据文件是否可读修改`posix_mode`的读写权限，同时会读取文件的用户名和组名作为`user_name`和
`group_name`；
而在*Unix*平台上，则会将文件读写权限和是否为符号链接或文件夹等信息写入`windows_attributes`，由于其没有`create_time`，所以会用
`modification_time`作为文件创建时间。\
在*Windows*平台上，软链接依然会更新`symbolic_link_target`。

# `Device`

# `Tar`

`Tar`模块是一个跨平台的`tar`归档文件处理模块，支持`tar`文件的创建、解压缩和读取。

  - 针对长目录的问题，有`POSIX 2001 PAX`和`GNU`两种解决方案。在读取时，会自动读取文件的头信息决定按哪种标准解读。
  - 此外，还支持了`GNU`的大文件大小存储方式和`PAX`的扩展头部。
  - 读取`tar`文件时会自动创建`sqlite3`内存数据库建立文件索引，支持随机读取文件。
